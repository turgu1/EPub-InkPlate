// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __OPTION_CONTROLLER_HPP__
#define __OPTION_CONTROLLER_HPP__

#include "global.hpp"
#include "controllers/event_mgr.hpp"

class OptionController
{
  private:
    static constexpr char const * TAG = "OptionController";

  public:
    OptionController();
    void key_event(EventMgr::KeyEvent key);
    void enter();
    void leave(bool going_to_deep_sleep = false);
};

#if __OPTION_CONTROLLER__
  OptionController option_controller;
#else
  extern OptionController option_controller;
#endif

#endif