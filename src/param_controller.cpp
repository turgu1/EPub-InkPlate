#define __PARAM_CONTROLLER__ 1
#include "param_controller.hpp"

#include "app_controller.hpp"
#include "epub.hpp"

static const char * TAG = "ParamController";

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