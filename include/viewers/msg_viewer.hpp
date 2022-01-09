// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"

#include "screen.hpp"

/**
 * @brief Message presentation class
 * 
 * This class supply simple alert/info messages presentation to the user.
 * 
 */
class MsgViewer {

  private:
    uint16_t width;
    static constexpr uint16_t HEIGHT  = 300;
    static constexpr uint16_t HEIGHT2 = 450;

    struct DotsZone {
      Pos pos;
      Dim dim;
      int16_t max_dot_count;
      int16_t dots_per_line;
    } dot_zone;

    int16_t dot_count;

    #if INKPLATE_6PLUS || TOUCH_TRIAL
      Pos ok_pos, cancel_pos;
      Dim buttons_dim;
      bool confirmation_required;
    #endif

  public:
    MsgViewer() {};

    enum MsgType { INFO, ALERT, BUG, BOOK, WIFI, NTP_CLOCK, CONFIRM };
    static char icon_char[7];

    void show(
      MsgType msg_type, 
      bool press_a_key, 
      bool clear_screen,
      const char * title, 
      const char * fmt_str, ...);
    
    bool confirm(const EventMgr::Event & event, bool & ok);

    //void show_progress(const char * title, ...);
    //void add_dot();
    void out_of_memory(const char * raison);
};

#if __MSG_VIEWER__
  MsgViewer msg_viewer;
#else
  extern MsgViewer msg_viewer;
#endif
