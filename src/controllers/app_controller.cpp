// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __APP_CONTROLLER__ 1
#include "controllers/app_controller.hpp"

#include "controllers/books_dir_controller.hpp"
#include "controllers/book_controller.hpp"
#include "controllers/param_controller.hpp"
#include "controllers/option_controller.hpp"
#include "controllers/event_mgr.hpp"
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
  current_ctrl = NONE;
  next_ctrl    = DIR;

  #if EPUB_LINUX_BUILD
    launch();
    event_mgr.loop(); // Will start gtk. Will not return.
  #else
    while (true) {
      if (next_ctrl != NONE) launch();
      event_mgr.loop();
    }
  #endif
}

void 
AppController::set_controller(Ctrl new_ctrl) 
{
  next_ctrl = new_ctrl;
}

void AppController::launch()
{
  #if EPUB_LINUX_BUILD
    if (next_ctrl == NONE) return;
  #endif
  
  Ctrl the_ctrl = next_ctrl;
  next_ctrl = NONE;

  if (((the_ctrl == LAST) && (last_ctrl[0] != current_ctrl)) || (the_ctrl != current_ctrl)) {

    switch (current_ctrl) {
      case DIR: books_dir_controller.leave(); break;
      case BOOK:     book_controller.leave(); break;
      case PARAM:   param_controller.leave(); break;
      case OPTION: option_controller.leave(); break;
      case NONE:
      case LAST:                              break;
    }

    Ctrl tmp = current_ctrl;
    current_ctrl = (the_ctrl == LAST) ? last_ctrl[0] : the_ctrl;

    if (the_ctrl == LAST) {
      for (int i = 1; i < LAST_COUNT; i++) last_ctrl[i - 1] = last_ctrl[i];
      last_ctrl[LAST_COUNT - 1] = DIR;
    }
    else {
      for (int i = 1; i < LAST_COUNT; i++) last_ctrl[i] = last_ctrl[i - 1];
      last_ctrl[0] = tmp;
    }

    switch (current_ctrl) {
      case DIR: books_dir_controller.enter(); break;
      case BOOK:     book_controller.enter(); break;
      case PARAM:   param_controller.enter(); break;
      case OPTION: option_controller.enter(); break;
      case NONE:
      case LAST:                              break;
    }
  }
}

void 
AppController::key_event(EventMgr::KeyEvent key)
{
  switch (current_ctrl) {
    case DIR: books_dir_controller.key_event(key); break;
    case BOOK:     book_controller.key_event(key); break;
    case PARAM:   param_controller.key_event(key); break;
    case OPTION: option_controller.key_event(key); break;
    case NONE:
    case LAST:                                     break;
  }
}

void
AppController::going_to_deep_sleep()
{
  switch (current_ctrl) {
    case DIR: books_dir_controller.leave(true); break;
    case BOOK:     book_controller.leave(true); break;
    case PARAM:   param_controller.leave(true); break;
    case OPTION: option_controller.leave(true); break;
    case NONE:
    case LAST:                                  break;
  }
}