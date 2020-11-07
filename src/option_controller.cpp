#define __OPTION_CONTROLLER__ 1
#include "option_controller.hpp"

#include "app_controller.hpp"

// static const char * TAG = "OptionController";

OptionController::OptionController()
{

}

void 
OptionController::enter()
{

}

void 
OptionController::leave()
{

}

void 
OptionController::key_event(EventMgr::KeyEvent key)
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
      app_controller.set_controller(AppController::LAST);
      break;
  }
}