// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PARAM_CONTROLLER__ 1
#include "controllers/param_controller.hpp"

#include "controllers/app_controller.hpp"
#include "models/epub.hpp"

ParamController::ParamController()
{

}

void 
ParamController::enter()
{

}

void 
ParamController::leave()
{

}

void 
ParamController::key_event(EventMgr::KeyEvent key)
{
  switch (key) {
    case EventMgr::KEY_LEFT:
      break;
    case EventMgr::KEY_RIGHT:
      break;
    case EventMgr::KEY_UP:
      break;
    case EventMgr::KEY_DOWN:
      break;
    case EventMgr::KEY_SELECT:
      break;
    case EventMgr::KEY_HOME:
      epub.close_file();
      app_controller.set_controller(AppController::DIR);
      break;
  }
}