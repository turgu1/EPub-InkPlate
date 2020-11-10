#ifndef __PARAM_CONTROLLER_HPP__
#define __PARAM_CONTROLLER_HPP__

#include "global.hpp"
#include "eventmgr.hpp"

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