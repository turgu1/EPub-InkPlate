// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __MSG_HPP__
#define __MSG_HPP__

#include <string>

/**
 * @brief Message presentation class
 * 
 * This class supply simple alert/info messages presentation to the user.
 * 
 */
class Msg {

  public:
    Msg();

    enum Severity { INFO, ALERT };

    void show(std::string msg, Severity severity);
    void hide();

    void show_progress_bars(std::string msg, uint8_t count);
    void set_progress_bar(uint8_t idx, uint8_t value);
    void hide_progress_bar();
};

#endif