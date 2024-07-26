// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __APP_CONTROLLER__ 1
#include "controllers/app_controller.hpp"

#include "controllers/books_dir_controller.hpp"
#include "controllers/book_controller.hpp"
#include "controllers/book_param_controller.hpp"
#include "controllers/option_controller.hpp"
#include "controllers/toc_controller.hpp"
#include "controllers/event_mgr.hpp"

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2
  #include "controllers/back_lit.hpp"
#endif

#include "screen.hpp"

AppController::AppController() : 
  current_ctrl(Ctrl::DIR),
     next_ctrl(Ctrl::NONE)
{
  for (int i = 0; i < LAST_COUNT; i++) {
    last_ctrl[i] = Ctrl::DIR;
  }
}

void
AppController::start()
{
  current_ctrl = Ctrl::NONE;
  next_ctrl    = Ctrl::DIR;

  #if EPUB_LINUX_BUILD
    launch();
    event_mgr.loop(); // Will start gtk. Will not return.
  #else
    while (true) {
      while (next_ctrl != Ctrl::NONE) launch();
      event_mgr.loop();
    }
  #endif
}

void 
AppController::set_controller(Ctrl new_ctrl) 
{
  LOG_D("===> set_controller()...");
  
  next_ctrl = new_ctrl;
}

void AppController::launch()
{
  #if EPUB_LINUX_BUILD
    if (next_ctrl == Ctrl::NONE) return;
  #endif
  
  Ctrl the_ctrl = next_ctrl;
  next_ctrl = Ctrl::NONE;

  if (((the_ctrl == Ctrl::LAST) && (last_ctrl[0] != current_ctrl)) || (the_ctrl != current_ctrl)) {

    switch (current_ctrl) {
      case Ctrl::DIR:     books_dir_controller.leave(); break;
      case Ctrl::BOOK:         book_controller.leave(); break;
      case Ctrl::PARAM:  book_param_controller.leave(); break;
      case Ctrl::OPTION:     option_controller.leave(); break;
      case Ctrl::TOC:           toc_controller.leave(); break;
      case Ctrl::NONE:
      case Ctrl::LAST:                                  break;
    }

    Ctrl tmp = current_ctrl;
    current_ctrl = (the_ctrl == Ctrl::LAST) ? last_ctrl[0] : the_ctrl;

    if (the_ctrl == Ctrl::LAST) {
      for (int i = 1; i < LAST_COUNT; i++) last_ctrl[i - 1] = last_ctrl[i];
      last_ctrl[LAST_COUNT - 1] = Ctrl::DIR;
    }
    else {
      for (int i = 1; i < LAST_COUNT; i++) last_ctrl[i] = last_ctrl[i - 1];
      last_ctrl[0] = tmp;
    }

    switch (current_ctrl) {
      case Ctrl::DIR:     books_dir_controller.enter(); break;
      case Ctrl::BOOK:         book_controller.enter(); break;
      case Ctrl::PARAM:  book_param_controller.enter(); break;
      case Ctrl::OPTION:     option_controller.enter(); break;
      case Ctrl::TOC:           toc_controller.enter(); break;
      case Ctrl::NONE:
      case Ctrl::LAST:                                  break;
    }
  }
}

void 
AppController::input_event(const EventMgr::Event & event)
{
  if (next_ctrl != Ctrl::NONE) launch();

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2
    if (event.kind == EventMgr::EventKind::PINCH_ENLARGE) {
      back_lit.adjust(event.dist);
      return;
    }
    else if (event.kind == EventMgr::EventKind::PINCH_REDUCE) {
      back_lit.adjust(-event.dist);
      return;
    }
  #endif

  switch (current_ctrl) {
    case Ctrl::DIR:     books_dir_controller.input_event(event); break;
    case Ctrl::BOOK:         book_controller.input_event(event); break;
    case Ctrl::PARAM:  book_param_controller.input_event(event); break;
    case Ctrl::OPTION:     option_controller.input_event(event); break;
    case Ctrl::TOC:           toc_controller.input_event(event); break;
    case Ctrl::NONE:
    case Ctrl::LAST:                                             break;
  }
}

void
AppController::going_to_deep_sleep()
{
  if (next_ctrl != Ctrl::NONE) launch();

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2
    back_lit.turn_off();
    touch_screen.shutdown();
  #endif

  switch (current_ctrl) {
    case Ctrl::DIR:     books_dir_controller.leave(true); break;
    case Ctrl::BOOK:         book_controller.leave(true); break;
    case Ctrl::PARAM:  book_param_controller.leave(true); break;
    case Ctrl::OPTION:     option_controller.leave(true); break;
    case Ctrl::TOC:           toc_controller.leave(true); break;
    case Ctrl::NONE:
    case Ctrl::LAST:                                      break;
  }
}
