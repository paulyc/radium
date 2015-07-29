// This file is not compiled directly, but #included into SoundProducer.cpp


#include <boost/version.hpp>
//#include <boost/thread/thread.hpp>
#include <boost/lockfree/queue.hpp>
//#include <boost/atomic.hpp>


#if (BOOST_VERSION < 100000) || ((BOOST_VERSION / 100 % 1000) < 58)
  #error "Boost too old or not found. Need at least 1.58.\n Quick fix: cd $HOME ; wget http://downloads.sourceforge.net/project/boost/boost/1.58.0/boost_1_58_0.tar.gz ; tar xvzf boost_1_58_0.tar.gz (that's it!)"
#endif


#include <QThread>
#include <QAtomicInt>

#include "../common/nsmtracker.h"
#include "../common/threading.h"
#include "../common/stacktoucher_proc.h"
#include "../common/settings_proc.h"
#include "../common/Semaphores.h"

#include "../common/OS_Player_proc.h"

#include "SoundProducer_proc.h"


#include "MultiCore_proc.h"

//#undef R_ASSERT
//#define R_ASSERT(a) do{ if(!(a)){ fflush(stderr);fprintf(stderr,">>>>>>>>>>>>>>>>>>> Assert failed: \"" # a "\". %s: " __FILE__":%d\n", __FUNCTION__, __LINE__);something_is_wrong=true;}}while(0)



static const int default_num_runners = 1;
static const char *settings_key = "num_cpus";

bool g_running_multicore = false; // Must be "false" initially since MultiCore is initialized after the mixer has started. This variable is protected by the player lock.

static volatile bool something_is_wrong = false;


static radium::Semaphore all_sp_finished;

static radium::Semaphore sp_ready;
static QAtomicInt num_sp_left(0);

#define MAX_NUM_SP 8192

static boost::lockfree::queue<SoundProducer*,boost::lockfree::capacity<MAX_NUM_SP>> ready_soundproducers;


#if 0
static bool sp_is_bus_provider(SoundProducer *sp, int bus_num){
  SoundPlugin *plugin = SP_get_plugin(sp);

  if (plugin->bus_descendant_type==IS_BUS_PROVIDER) {
    Smooth *smooth = &plugin->bus_volume[bus_num];
    if (SMOOTH_are_we_going_to_modify_target_when_mixing_sounds_questionmark(smooth))
      return true;
  }

  return false;
}
#endif

static void schedule_sp(SoundProducer *sp){
  while(!ready_soundproducers.bounded_push(sp))
    ;
  //fprintf(stderr, "*** inline scheduling %s (from %s)\n",sp->_plugin->patch==NULL?"<null>":sp->_plugin->patch->name,parent->_plugin->patch==NULL?"<null>":parent->_plugin->patch->name);
  //fflush(stderr);
  sp_ready.signal();
}

static void dec_sp_dependency(const SoundProducer *parent, SoundProducer *sp){
  if (!sp->num_dependencies_left.deref())
    schedule_sp(sp);
}

static void process_multicore(SoundProducer *sp, int64_t time, int num_frames, bool process_plugins){
  R_ASSERT(g_running_multicore);
  
  double start_time = monotonic_seconds();
  {
    sp->RT_process(time, num_frames, process_plugins);
  }
  double duration = monotonic_seconds() - start_time;
  if (duration > sp->running_time)
    sp->running_time = duration;

  if (!num_sp_left.deref()){
    //printf("num_left1: %d\n",0);
    R_ASSERT(sp->dependants.is_empty());
    all_sp_finished.signal();
    return;
  }

  //int num_left = num_sp_left;
  //printf("num_left2: %d\n",num_left);

  for(auto sp_dep : sp->dependants)
    dec_sp_dependency(sp, sp_dep);
     
  if (sp->_plugin->bus_descendant_type==IS_BUS_PROVIDER) {
    SoundProducer *bus1,*bus2;
    MIXER_get_buses(bus1,bus2);
    if (bus1!=NULL)
      dec_sp_dependency(sp, bus1);
    if (bus2!=NULL)
      dec_sp_dependency(sp, bus2);
  }
}


namespace{

  
struct Runner : public QThread {
  Q_OBJECT

public:
  radium::Semaphore can_start_main_loop;
  volatile bool must_exit;

  int64_t time;
  int num_frames;
  bool process_plugins;

  Runner()
    : must_exit(false)
  {
    QObject::connect(this, SIGNAL(finished()), this, SLOT(onFinished()));
    start(QThread::TimeCriticalPriority); // The priority shouldn't matter though since PLAYER_acquire_same_priority() is called inside run().
  }

#if 0
  ~Runner() {
    R_ASSERT(must_exit==true);
    //sp_ready.signal();
    wait();
  }
#endif

  void run(){
    AVOIDDENORMALS;

    touch_stack();
        
    THREADING_acquire_player_thread_priority();

    can_start_main_loop.wait();

    while(true){

      sp_ready.wait();

      if (must_exit) {
        sp_ready.signal();
        break;
      }
      
      SoundProducer *sp = NULL;
      bool success = ready_soundproducers.pop(sp);
      
      R_ASSERT(success);
      R_ASSERT(sp!=NULL);

      //  fprintf(stderr,"   Processing %p: %s %d\n",sp,sp->_plugin->patch==NULL?"<null>":sp->_plugin->patch->name,int(sp->is_processed));
      //fflush(stderr);

      R_ASSERT(int(sp->is_processed)==0);
      sp->is_processed.ref();
      
      process_multicore(sp, time, num_frames, process_plugins);

    } // end while


    THREADING_drop_player_thread_priority();    
  }

private slots:
  void onFinished(){
    //printf("\n\n\n ***************** FINISHED ****************** \n\n\n\n");
    delete this;
  }

};
#include "mMultiCore.cpp"
}


static int g_num_runners = 0;
static Runner **g_runners = NULL;



void MULTICORE_run_all(radium::Vector<SoundProducer*> *sp_all, int64_t time, int num_frames, bool process_plugins){

  R_ASSERT(g_running_multicore);

  if (sp_all->size()==0)
    return;

  if (sp_all->size() >= MAX_NUM_SP){
    RT_message("Can't play since there are too many sound objects (%d). (this limit can be increased, but it's probably more likely that this message is shown because there is a bug in the program.)", sp_all->size());
    return;
  }

  int num_ready_sp = 0;


  // 1. Initialize threads

  for(int i=0;i<g_num_runners;i++){
    g_runners[i]->time = time;
    g_runners[i]->num_frames = num_frames;
    g_runners[i]->process_plugins = process_plugins;
  }


  
  // 2. initialize soundproducers
  
  num_sp_left = sp_all->size();
  //  fprintf(stderr,"**************** STARTING %d\n",sp_all->size());
  //fflush(stderr);

  SoundProducer *bus1,*bus2;
  MIXER_get_buses(bus1,bus2);

  
  for (SoundProducer *sp : *sp_all)
    sp->num_dependencies_left = sp->num_dependencies;

  for (SoundProducer *sp : *sp_all) {
    sp->is_processed=0;

    if (sp->_plugin->bus_descendant_type==IS_BUS_PROVIDER) {
      if (bus1!=NULL)
        bus1->num_dependencies_left.ref();
      if (bus2!=NULL)
        bus2->num_dependencies_left.ref();
    }
  }

  
#if 0
  fprintf(stderr, "bus1: %p, bus2: %p\n",bus1,bus2);
  fflush(stderr);

  int num=0;

  for (SoundProducer *sp : *sp_all){
    fprintf(stderr,"%d: sp: %p (%s). num_dep: %d, num_dep_left: %d: num_dependant: %d, bus provider: %d\n",num++,sp,sp->_plugin->patch==NULL?"<null>":sp->_plugin->patch->name,sp->num_dependencies,int(sp->num_dependencies_left), sp->dependants.size(), sp->_plugin->bus_descendant_type==IS_BUS_PROVIDER);
    fflush(stderr);
  }
#endif



  // 3. start threads;

  if (bus1!=NULL && int(bus1->num_dependencies_left)==0){
    //fprintf(stderr,"Scheduling bus1.\n");
    num_ready_sp++;
    schedule_sp(bus1);
  }
  
  if (bus2!=NULL && int(bus2->num_dependencies_left)==0){
    //fprintf(stderr,"Scheduling bus2.\n");
    num_ready_sp++;
    schedule_sp(bus2);
  }
  
  for (SoundProducer *sp : *sp_all)
    if (sp->num_dependencies==0 && sp!=bus1 && sp!=bus2){
      num_ready_sp++;
      //fprintf(stderr,"Scheduling %p: %s\n",sp,sp->_plugin->patch==NULL?"<null>":sp->_plugin->patch->name);
      //fflush(stderr);
      schedule_sp(sp);
    }



  // 4. wait.

  R_ASSERT(num_ready_sp > 0);

  all_sp_finished.wait();

  if(something_is_wrong){
    fflush(stderr);
    abort();
  }

}



#if 0
void MULTICORE_stop(void){
  R_ASSERT(running_runners==NULL);

  while(free_runners != NULL){
    Runner *next = free_runners->next;
    delete free_runners;
    free_runners = next;
  }

  free_runners = NULL;
}
#endif


int MULTICORE_get_num_threads(void){
  return SETTINGS_read_int(settings_key, default_num_runners);
}


void MULTICORE_set_num_threads(int num_new_runners){
  R_ASSERT(num_new_runners >= 1);

  if (num_new_runners==g_num_runners)
    return;

  if (SETTINGS_read_int(settings_key, default_num_runners) != num_new_runners)
    SETTINGS_write_int(settings_key, num_new_runners);

    
  int num_old_runners = g_num_runners;
  Runner **old_runners = g_runners;
  
  Runner **new_runners = (Runner**)calloc(num_new_runners,sizeof(Runner*));

  for(int i=0 ; i < num_new_runners ; i++)
    new_runners[i]=new Runner;

  PLAYER_lock(); {

    for(int i=0 ; i < num_new_runners ; i++)
      new_runners[i]->can_start_main_loop.signal();

    g_num_runners = num_new_runners;
    g_runners = new_runners;
    
    if (g_num_runners == 1)
      g_running_multicore = false;
    else
      g_running_multicore = true;

    for(int i=0 ; i < num_old_runners ; i++)
      old_runners[i]->must_exit = true;

  } PLAYER_unlock();


  free(old_runners);
}

void MULTICORE_shut_down(void){
  
}

void MULTICORE_init(void){

  int num_new_runners = SETTINGS_read_int(settings_key, default_num_runners);

  MULTICORE_set_num_threads(num_new_runners);
}
