// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __MSG_VIEWER_HPP__
#define __MSG_VIEWER_HPP__

#include "screen.hpp"

#include <string>

/**
 * @brief Message presentation class
 * 
 * This class supply simple alert/info messages presentation to the user.
 * 
 */
class MsgViewer {

  private:
    uint16_t width;
    static constexpr uint16_t HEIGHT  = 240;
    static constexpr uint16_t HEIGHT2 = 400;

    struct DotsZone {
      Pos pos;
      Dim dim;
      int16_t max_dot_count;
      int16_t dots_per_line;
    } dot_zone;

    int16_t dot_count;

  public:
    MsgViewer() {};

    enum Severity       { INFO, ALERT, BUG, BOOK, WIFI };
    static char icon_char[5];

    void show(
      Severity severity, 
      bool press_a_key, 
      bool clear_screen,
      const char * title, 
      const char * fmt_str, ...);
    
    void show_progress(const char * title, ...);
    void add_dot();
    void out_of_memory(const char * raison);
};

#if __MSG_VIEWER__
  MsgViewer msg_viewer;
#else
  extern MsgViewer msg_viewer;
#endif

#endif