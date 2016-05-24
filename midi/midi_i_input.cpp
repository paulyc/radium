/* Copyright 2000 Kjetil S. Matheussen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. */


#include <boost/version.hpp>
#if (BOOST_VERSION < 100000) || ((BOOST_VERSION / 100 % 1000) < 58)
  #error "Boost too old. Need at least 1.58.\n Quick fix: cd $HOME ; wget http://downloads.sourceforge.net/project/boost/boost/1.60.0/boost_1_60_0.tar.bz2 ; tar xvjf boost_1_60_0.tar.bz2 (that's it!)"
#endif
#include <boost/lockfree/queue.hpp>



#include "nsmtracker.h"

#include "midi_i_plugin.h"
#include "midi_i_plugin_proc.h"
#include "midi_proc.h"

#include "../common/notes_proc.h"
#include "../common/blts_proc.h"
#include "../common/OS_Ptask2Mtask_proc.h"
#include "../common/player_proc.h"
#include "../common/patch_proc.h"
#include "../common/placement_proc.h"
#include "../common/time_proc.h"
#include "../common/hashmap_proc.h"
#include "../common/undo.h"
#include "../common/undo_notes_proc.h"
#include "../common/settings_proc.h"
#include "../audio/Mixer_proc.h"
#include "../common/OS_Player_proc.h"
#include "../common/visual_proc.h"
#include "../common/Mutex.hpp"

#include "midi_i_input_proc.h"

static DEFINE_ATOMIC(uint32_t, g_msg) = 0;

static DEFINE_ATOMIC(struct Patch *, g_through_patch) = NULL;

// TODO: This isn't always working properly. Going to change rtmidi API.

extern const char *NotesTexts3[131];


#define PACK_MIDI_MSG(a,b,c) ( (a&0xf0)<<16 | b<<8 | c)

typedef struct _midi_event_t{
  struct _midi_event_t *next;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  STime blocktime;
  uint32_t msg;
} midi_event_t;

static radium::Mutex g_midi_event_mutex;
static midi_event_t *g_midi_events = NULL;
static midi_event_t *g_recorded_midi_events = NULL;
static midi_event_t *g_last_recorded_midi_event = NULL;

static midi_event_t *get_midi_event(void){

  if (g_midi_events==NULL) {
    
    midi_event_t *midi_events = (midi_event_t*)V_calloc(1024, sizeof(midi_event_t));
    int i;
    for(i=1;i<1023;i++)
      midi_events[i].next = &midi_events[i+1];
    
    g_midi_events = &midi_events[1];
    
    return &midi_events[0];
    
  }else {

    midi_event_t *ret = g_midi_events;
    g_midi_events = ret->next;
    return ret;

  }
}

static void record_midi_event(uint32_t msg){

  radium::ScopedMutex lock(&g_midi_event_mutex);
  
  if (root==NULL || root->song==NULL || root->song->tracker_windows==NULL || root->song->tracker_windows->wblock==NULL)
    return;

  struct Tracker_Windows *window = root->song->tracker_windows;
  struct WBlocks *wblock = window->wblock;
  struct WTracks *wtrack = wblock->wtrack;
  struct Tracks *track = wtrack->track;
  
  midi_event_t *midi_event = get_midi_event();

  midi_event->next = NULL;
  
  midi_event->wblock    = wblock;
  midi_event->wtrack    = wtrack;
  midi_event->blocktime = R_MAX(0, MIXER_get_accurate_radium_time() - ATOMIC_GET(pc->seqtime)); // TODO/FIX: This can fail if pc->seqtime is not updated at the same time as 'jackblock_cycle_start_stime' in Mixer.cpp.
  midi_event->msg       = msg;

  if (ATOMIC_GET(track->is_recording) == false){
    GFX_ScheduleEditorRedraw();
    ATOMIC_SET(track->is_recording, true);
  }

  //printf("Rec %d: %x, %x, %x\n",(int)midi_event->blocktime,cc,data1,data2);

  if (g_recorded_midi_events==NULL)
    g_recorded_midi_events = midi_event;
  else
    g_last_recorded_midi_event->next = midi_event;

  g_last_recorded_midi_event = midi_event;
}


static midi_event_t *find_midievent_end_note(midi_event_t *midi_event, int notenum_to_find, STime starttime_of_note){
  while(midi_event!=NULL){
    if (midi_event->wblock!=NULL) {
      
      uint32_t msg = midi_event->msg;
      int cc = msg>>16;
      int notenum = (msg>>8)&0xff;
      int volume = msg&0xff;
      
      if (cc==0x80 || volume==0){
        if (notenum==notenum_to_find && midi_event->blocktime > starttime_of_note)
          return midi_event;
      }
    }
    
    midi_event = midi_event->next;
  }

  return NULL;
}


void MIDI_insert_recorded_midi_events(void){
  radium::ScopedMutex lock(&g_midi_event_mutex); // Will wait here in case record_midi_event is not finished. Probably a very rare situation.
  
  midi_event_t *midi_event = g_recorded_midi_events;

  if (midi_event==NULL)
    return;

  hash_t *track_set = HASH_create(8);
  
  Undo_Open();{

    while(midi_event != NULL){
      midi_event_t *next = midi_event->next;

      if (midi_event->wblock!=NULL) {

        struct Blocks *block = midi_event->wblock->block;
        struct Tracks *track = midi_event->wtrack->track;
        ATOMIC_SET(track->is_recording, false);
        
        char *key = (char*)talloc_format("%x",midi_event->wtrack);
        if (HASH_has_key(track_set, key)==false){

          ADD_UNDO(Notes(root->song->tracker_windows,
                     block,
                     track,
                     midi_event->wblock->curr_realline
                         ));
          HASH_put_int(track_set, key, 1);
        }
        

        STime time = midi_event->blocktime;
        uint32_t msg = midi_event->msg;
            
        int cc = (msg>>16)&0xf0; // remove channel
        int notenum = (msg>>8)&0xff;
        int volume = msg&0xff;

        // add note
        if (cc==0x90 && volume>0) {
        
          Place place = STime2Place(block,time);
          Place endplace;
          Place *endplace_p;
        
          midi_event_t *midi_event_endnote = find_midievent_end_note(next,notenum,time);
          if (midi_event_endnote!=NULL){
            midi_event_endnote->wblock = NULL; // only use it once
            endplace = STime2Place(block,midi_event_endnote->blocktime);
            endplace_p = &endplace;
          }else
            endplace_p = NULL;
        
          InsertNote(midi_event->wblock,
                     midi_event->wtrack,
                     &place,
                     endplace_p,
                     notenum,
                     (float)volume * MAX_VELOCITY / 127.0f,
                     true
                     );
        }
      
      }
      
      // remove event
      midi_event->next = g_midi_events;
      g_midi_events = midi_event;
      
      
      // iterate next
      midi_event = next;
    }
    
  }Undo_Close();

  
  g_recorded_midi_events = NULL;
  g_last_recorded_midi_event = NULL;
}

typedef struct {
  int32_t deltatime;
  uint32_t msg;
} play_buffer_event_t;

static boost::lockfree::queue<play_buffer_event_t, boost::lockfree::capacity<8000> > g_play_buffer;

static void add_event_to_play_buffer(int cc,int data1,int data2){
  play_buffer_event_t event;

  event.deltatime = 0;
  event.msg = PACK_MIDI_MSG(cc,data1,data2);

  while (!g_play_buffer.bounded_push(event));
}

void RT_MIDI_handle_play_buffer(void){
  struct Patch *patch = ATOMIC_GET(g_through_patch);
  
  while (!g_play_buffer.empty()) {
    play_buffer_event_t event;
    g_play_buffer.pop(event);

    if(patch!=NULL){
      
      uint32_t msg = event.msg;
      
      uint8_t data[3] = {(uint8_t)MIDI_msg_byte1(msg), (uint8_t)MIDI_msg_byte2(msg), (uint8_t)MIDI_msg_byte3(msg)};
      
      RT_MIDI_send_msg_to_patch((struct Patch*)patch, data, 3, -1);
    }
  }
}


static bool g_record_accurately_while_playing = true;

bool MIDI_get_record_accurately(void){
  return g_record_accurately_while_playing;
}

void MIDI_set_record_accurately(bool accurately){
  SETTINGS_write_bool("record_midi_accurately", accurately);
  g_record_accurately_while_playing = accurately;
}

static bool g_record_velocity = true;

bool MIDI_get_record_velocity(void){
  return g_record_velocity;
}

void MIDI_set_record_velocity(bool doit){
  printf("doit: %d\n",doit);
  SETTINGS_write_bool("always_record_midi_velocity", doit);
  g_record_velocity = doit;
}

void MIDI_InputMessageHasBeenReceived(int cc,int data1,int data2){
  //printf("got new message. on/off:%d. Message: %x,%x,%x\n",(int)root->editonoff,cc,data1,data2);
  //static int num=0;
  //num++;

  if(cc==0xf0 || cc==0xf7) // sysex not supported
    return;

  bool isplaying = is_playing();

  uint32_t msg = MIDI_msg_pack3(cc, data1, data2);
  int len = MIDI_msg_len(msg);
  if (len<1 || len>3)
    return;
  
  if(ATOMIC_GET(g_through_patch)!=NULL)
    add_event_to_play_buffer(cc, data1, data2);
  
  if (g_record_accurately_while_playing && isplaying) {
    
    if(cc>=0x80 && cc<0xa0)
      if (ATOMIC_GET(root->editonoff))
        record_midi_event(msg);

  } else {

    if((cc&0xf0)==0x90 && data2!=0)
      if (ATOMIC_COMPARE_AND_SET_UINT32(g_msg, 0, msg)==false) {
        // printf("Playing to fast. Skipping note %u from MIDI input.\n",msg); // don't want to print in realtime thread
      }
  }
}

// This is safe. A patch is never deleted.
void MIDI_SetThroughPatch(struct Patch *patch){
  //printf("Sat new patch %p\n",patch);
  if(patch!=NULL)
    ATOMIC_SET(g_through_patch, patch);
}


// called very often
void MIDI_HandleInputMessage(void){
  // should be a memory barrier here somewhere.

  uint32_t msg = ATOMIC_GET(g_msg); // Hmm, should have an ATOMIC_COMPAREFALSE_AND_SET function.
  
  if (msg!=0) {

    ATOMIC_SET(g_msg, 0);

    if(ATOMIC_GET(root->editonoff)){
      float velocity = -1.0f;
      if (g_record_velocity)
        velocity = (float)MIDI_msg_byte3(msg) / 127.0;
      //printf("velocity: %f, byte3: %d\n",velocity,MIDI_msg_byte3(msg));
      InsertNoteCurrPos(root->song->tracker_windows,MIDI_msg_byte2(msg), false, velocity);
      root->song->tracker_windows->must_redraw = true;
    }
  }
}

void MIDI_input_init(void){
  radium::ScopedMutex lock(&g_midi_event_mutex);
    
  MIDI_set_record_accurately(SETTINGS_read_bool("record_midi_accurately", true));
  MIDI_set_record_velocity(SETTINGS_read_bool("always_record_midi_velocity", false));
  
  midi_event_t *midi_event = get_midi_event();
  
  midi_event->next = g_midi_events;
  
  g_midi_events = midi_event;
}
