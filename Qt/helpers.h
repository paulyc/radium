#ifndef RADIUM_QT_HELPERS
#define RADIUM_QT_HELPERS

#include <QMessageBox>
#include <QMenu>
#include <QTimer>
#include <QTime>
#include <QMainWindow>
#include <QSplashScreen>
#include <QApplication>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QPointer>

#include "../OpenGL/Widget_proc.h"
#include "../common/keyboard_focus_proc.h"
#include "../common/visual_proc.h"

#include "../api/api_gui_proc.h"
#include "../api/api_proc.h"

#define PUT_ON_TOP 0

extern bool g_radium_runs_custom_exec;

extern void set_editor_focus(void);

extern QVector<QWidget*> g_static_toplevel_widgets;

extern QMainWindow *g_main_window;
extern QSplashScreen *g_splashscreen;

static QWidget *get_current_parent(void){

  if (QApplication::activeModalWidget()!=NULL)
    return QApplication::activeModalWidget();

  if (QApplication::activePopupWidget()!=NULL)
    return QApplication::activePopupWidget();
    
  if (QApplication::activeWindow()!=NULL)
    return QApplication::activeWindow();
  
  QWidget *mixer_strips_widget = MIXERSTRIPS_get_curr_widget();
  if (mixer_strips_widget!=NULL)
    return mixer_strips_widget;
  
  QWidget *curr_under = QApplication::widgetAt(QCursor::pos());
  if (g_static_toplevel_widgets.contains(curr_under))
    return curr_under;

  return g_main_window;
    
    /*
    if (curr_under != NULL)
      return curr_under;
    else if (QApplication::activeWindow()!=NULL)
      return QApplication::activeWindow();
    else
      return g_main_window;
    */
}

namespace radium{
  struct ASMTimer : public QTimer{
    QTime time;
    bool left_mouse_is_down = false;
    
    ASMTimer(QWidget *parent_)
      :QTimer(parent_)
    {
      time.start();
      setInterval(10);
    }

    void timerEvent ( QTimerEvent * e ){
      if (QGuiApplication::mouseButtons()==Qt::LeftButton)
        left_mouse_is_down = true;
      else if (left_mouse_is_down){
        left_mouse_is_down = false;
        time.restart();
      }
        
      //printf("  MOUSE DOWN: %d\n", QGuiApplication::mouseButtons()==Qt::LeftButton);
    }

    bool mouseWasDown(void){
      if(left_mouse_is_down==true)
        return true;
      if (time.elapsed() < 500)
        return true;
      return false;
    }
  };

  class VerticalScroll : public QScrollArea {
    //    Q_OBJECT;
    
  public:

    QVBoxLayout *layout;    
    
    VerticalScroll(QWidget *parent_)
      :QScrollArea(parent_)
    {
      setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
      setWidgetResizable(true);
      
      QWidget *contents = new QWidget(this);
      
      layout = new QVBoxLayout(contents);
      layout->setSpacing(1);

      contents->setLayout(layout);
      
      setWidget(contents);    
    }
    
    void addWidget(QWidget *widget_){
      layout->addWidget(widget_);
    }
    
    void removeWidget(QWidget *widget_){
      layout->removeWidget(widget_);
    }
  };


}


// Why doesn't Qt provide this one?
namespace{
template <typename T>
struct ScopedQPointer : public QPointer<T>{
  T *_widget;
  ScopedQPointer(T *widget)
    : QPointer<T>(widget)
    , _widget(widget)
  {}
  ~ScopedQPointer(){
    printf("Deleting scoped pointer widget %p\n",_widget);
    delete _widget;
  }
};
/*
template <typename T>
struct ScopedQPointer {

  QPointer<T> _box;
  
  MyQPointer(T *t){
    _box = t;
    //printf("MyQPointer created %p for %p %p %p\n", this, _box, _box.data(), t);
  }

  ~MyQPointer(){    
    //printf("MyQPointer %p deleted (_box: %p %p)\n", this, _box, _box.data());
    delete data();
  }

  inline T* data() const
  {
    return _box.data();
  }
  inline T* operator->() const
  { return data(); }
  inline T& operator*() const
  { return *data(); }
  inline operator T*() const
  { return data(); }
};
*/
}


struct MyQMessageBox : public QMessageBox {
  bool _splashscreen_visible;

  // Prevent stack allocation. Not sure, but I think get_current_parent() could return something that could be deleted while calling exec.
  MyQMessageBox(const MyQMessageBox &) = delete;
  MyQMessageBox & operator=(const MyQMessageBox &) = delete;
  MyQMessageBox(MyQMessageBox &&) = delete;
  MyQMessageBox & operator=(MyQMessageBox &&) = delete;

  //
  // !!! parent must/should be g_main_window unless we use ScopedQPointer !!!
  //
  static MyQMessageBox *create(QWidget *parent = NULL){
    return new MyQMessageBox(parent);
  }
  
 private:  
    
  MyQMessageBox(QWidget *parent_ = NULL)
    : QMessageBox(parent_!=NULL ? parent_ : get_current_parent())
  {
    setWindowModality(Qt::ApplicationModal);
    //setWindowModality(Qt::NonModal);
    setWindowFlags(Qt::Window | Qt::Tool);
  }

 public:
  
  ~MyQMessageBox(){
    printf("MyQMessageBox %p deleted\n", this);
  }
  
  void showEvent(QShowEvent *event_){
    _splashscreen_visible = g_splashscreen!=NULL && g_splashscreen->isVisible();

    if (_splashscreen_visible)
      g_splashscreen->hide();

    QMessageBox::showEvent(event_);
  }
  
  void hideEvent(QHideEvent *event_) {
    if (_splashscreen_visible && g_splashscreen!=NULL)
      g_splashscreen->show();

    QMessageBox::hideEvent(event_);
  }
  
};

namespace radium{
  struct RememberGeometry{
    QByteArray geometry;
    bool has_stored_geometry = false;

    void save(QWidget *widget){
      geometry = widget->saveGeometry();
      has_stored_geometry = true;
    }

    void restore(QWidget *widget){
      if (has_stored_geometry)
        widget->restoreGeometry(geometry);
    }

    void remember_geometry_setVisible_override_func(QWidget *widget, bool visible) {
      //printf("   Set visible %d\n",visible);
      
      if (!visible){

        save(widget);
        
      } else {
        
        restore(widget);        
      }
      
      //QDialog::setVisible(visible);    
    }
  };
}

  
struct RememberGeometryQDialog : public QDialog {

  radium::RememberGeometry remember_geometry;

  static int num_open_dialogs;

#if PUT_ON_TOP

  struct Timer : public QTimer {
    bool was_visible = false;
    
    QWidget *parent;
    Timer(QWidget *parent)
      :parent(parent)
    {
      setInterval(200);
      start();      
    }

    ~Timer(){
      if (was_visible)
        RememberGeometryQDialog::num_open_dialogs--;
    }
      

    void timerEvent ( QTimerEvent * e ){
      //printf("Raising parent\n");
      bool is_visible = parent->isVisible();
      
      if(is_visible && !was_visible) {
        was_visible = true;
        RememberGeometryQDialog::num_open_dialogs++;
      }
          
      if(!is_visible && was_visible) {
        was_visible = false;
        RememberGeometryQDialog::num_open_dialogs--;
      }
          
      if (is_visible && RememberGeometryQDialog::num_open_dialogs==1)
        parent->raise(); // This is probably a bad idea.
    }
  };
  

  Timer timer;
#endif
  
public:
  RememberGeometryQDialog(QWidget *parent_)
    : QDialog(parent_!=NULL ? parent_ : get_current_parent(), Qt::Window | Qt::Tool)
#if PUT_ON_TOP
    , timer(this)
#endif
  {    
    //QDialog::setWindowModality(Qt::ApplicationModal);
    //setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  }
  void setVisible(bool visible) override {      
    remember_geometry.remember_geometry_setVisible_override_func(this, visible);

    QDialog::setVisible(visible);    
  }

};

struct GL_PauseCaller{
  GL_PauseCaller(){
    bool locked = GL_maybeLock();
    GL_pause_gl_thread_a_short_while();
    if (locked)
      GL_unlock();      
  }
};


namespace radium{
struct ScopedExec{
  bool _lock;
  
  ScopedExec(bool lock=true)
    : _lock(lock)
  {      
    obtain_keyboard_focus();
    
    g_radium_runs_custom_exec = true;
    
    GFX_HideProgress();
    
    if (_lock)  // GL_lock <strike>is</strike> was needed when using intel gfx driver to avoid crash caused by opening two opengl contexts simultaneously from two threads.
      GL_lock();
  }
  
  ~ScopedExec(){
    if (_lock)
      GL_unlock();
    
    GFX_ShowProgress();
    
    g_radium_runs_custom_exec = false;
    
    release_keyboard_focus();
  }
};
}





static inline int safeExec(QMessageBox *widget){
  R_ASSERT_RETURN_IF_FALSE2(g_radium_runs_custom_exec==false, 0);
      
  radium::ScopedExec scopedExec;

  return widget->exec();
}

static inline int safeExec(QMessageBox &widget){
  R_ASSERT_RETURN_IF_FALSE2(g_radium_runs_custom_exec==false, 0);

  radium::ScopedExec scopedExec;

  return widget.exec();
}

static inline int safeExec(QDialog *widget){
  R_ASSERT_RETURN_IF_FALSE2(g_radium_runs_custom_exec==false, 0);

  radium::ScopedExec scopedExec;

  return widget->exec();
}

static inline QAction *safeExec(QMenu *widget){
  QAction *ret;

  // safeExec might be called recursively if you double click right mouse button to open a pop up menu. Seems like a bug or design flaw in Qt.
#if !defined(RELEASE)
  if (g_radium_runs_custom_exec==true)
    GFX_Message(NULL, "Already runs custom exec");
#endif
  
  if (doModalWindows()) {
    radium::ScopedExec scopedExec;
    return widget->exec(QCursor::pos());
  }else{
    radium::ScopedExec scopedExec(false);
    GL_lock();{
      GL_pause_gl_thread_a_short_while();
    }GL_unlock();    
    return widget->exec(QCursor::pos());
  }
  return ret;
}

static inline void safeShow(QWidget *widget){  
  GL_lock(); {
    GL_pause_gl_thread_a_short_while();
    widget->show();
    widget->raise();
    widget->activateWindow();
  }GL_unlock();
}

static inline void safeShowOrExec(QDialog *widget){
  if (doModalWindows())
    safeExec(widget);
  else
    safeShow(widget);
}

#endif // RADIUM_QT_HELPERS
