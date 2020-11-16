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

    enum Severity       { INFO, ALERT, BUG };
    static char icon_char[3];

    static void show(Severity severity, const char * fmt_str, ...);
    void hide();

    void show_progress_bars(std::string msg, uint8_t count);
    void set_progress_bar(uint8_t idx, uint8_t value);
    void hide_progress_bar();
};

#endif