// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __APP_CONTROLLER__ 1
#include "app_controller.hpp"

#include "books_dir_controller.hpp"
#include "book_controller.hpp"
#include "param_controller.hpp"
#include "option_controller.hpp"
#include "eventmgr.hpp"
#include "screen.hpp"

AppController::AppController()
{
  current_ctrl = DIR;
  for (int i = 0; i < LAST_COUNT; i++) {
    last_ctrl[i] = DIR;
  }

}

void
AppController::start()
{
  books_dir_controller.enter();

  event_mgr.start_loop();
}

void 
AppController::set_controller(Ctrl new_ctrl) 
{
  if (((new_ctrl == LAST) && (last_ctrl[0] != current_ctrl)) || (new_ctrl != current_ctrl)) {

    switch (current_ctrl) {
      case DIR:       books_dir_controller.leave(); break;
      case BOOK:     book_controller.leave(); break;
      case PARAM:   param_controller.leave(); break;
      case OPTION: option_controller.leave(); break;
      case LAST:                              break;
    }

    Ctrl tmp = current_ctrl;
    current_ctrl = (new_ctrl == LAST) ? last_ctrl[0] : new_ctrl;

    if (new_ctrl == LAST) {
      for (int i = 1; i < LAST_COUNT; i++) last_ctrl[i - 1] = last_ctrl[i];
      last_ctrl[LAST_COUNT - 1] = DIR;
    }
    else {
      for (int i = 1; i < LAST_COUNT; i++) last_ctrl[i] = last_ctrl[i - 1];
      last_ctrl[0] = tmp;
    }

    switch (current_ctrl) {
      case DIR:       books_dir_controller.enter(); break;
      case BOOK:     book_controller.enter(); break;
      case PARAM:   param_controller.enter(); break;
      case OPTION: option_controller.enter(); break;
      case LAST:                              break;
    }
  }
}

void 
AppController::key_event(EventMgr::KeyEvent key)
{
  switch (current_ctrl) {
    case DIR:       books_dir_controller.key_event(key); break;
    case BOOK:     book_controller.key_event(key); break;
    case PARAM:   param_controller.key_event(key); break;
    case OPTION: option_controller.key_event(key); break;
    case LAST:                                     break;
  }
}