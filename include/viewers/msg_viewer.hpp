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
    static constexpr uint16_t WIDTH  = Screen::WIDTH - 60;
    static constexpr uint16_t HEIGHT = 240;

  public:
    MsgViewer();

    enum Severity       { INFO, ALERT, BUG, BOOK };
    static char icon_char[4];

    static void show(
      Severity severity, 
      bool press_a_key, 
      bool clear_screen,
      const char * title, 
      const char * fmt_str, ...);
    
    static void show_progress_bar(const char * title, ...);
    static void set_progress_bar(uint8_t value);
};

#endif