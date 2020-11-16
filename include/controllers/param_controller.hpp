// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __PARAM_CONTROLLER_HPP__
#define __PARAM_CONTROLLER_HPP__

#include "global.hpp"
#include "controllers/event_mgr.hpp"

class ParamController
{
  private:
    static constexpr char const * TAG = "ParamController";

  public:
    ParamController();
    void key_event(EventMgr::KeyEvent key);
    void enter();
    void leave();
};

#if __PARAM_CONTROLLER__
  ParamController param_controller;
#else
  extern ParamController param_controller;
#endif

#endif