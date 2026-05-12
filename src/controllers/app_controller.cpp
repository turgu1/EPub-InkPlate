// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __APP_CONTROLLER__ 1
#include "controllers/app_controller.hpp"

#include "controllers/book_controller.hpp"
#include "controllers/book_param_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "controllers/common_actions.hpp"
#include "controllers/event_mgr.hpp"
#include "controllers/option_controller.hpp"
#include "controllers/toc_controller.hpp"

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
  #include "controllers/back_lit.hpp"
#endif

#include "screen.hpp"

AppController::AppController() : currentCtrl(Ctrl::DIR), nextCtrl(Ctrl::NONE) {
  for (int i = 0; i < LAST_COUNT; ++i) {
    lastCtrl[i] = Ctrl::DIR;
  }
}

auto AppController::start() -> void {
  currentCtrl = Ctrl::NONE;
  nextCtrl    = Ctrl::DIR;

  #if EPUB_LINUX_BUILD
    launch();
    eventMgr.loop(); // Will start gtk. Will not return.
  #else
    while (true) {
      while (nextCtrl != Ctrl::NONE) launch();
      eventMgr.loop();
    }
  #endif
}

auto AppController::startRegression(Ctrl initialCtrl) -> void {
  currentCtrl = Ctrl::NONE;
  nextCtrl    = initialCtrl;

  for (int i = 0; i < LAST_COUNT; ++i) {
    lastCtrl[i] = Ctrl::DIR;
  }

  launch();
}

auto AppController::setController(Ctrl newCtrl) -> void {
  LOG_D("===> setController()...");

  nextCtrl = newCtrl;
}

auto AppController::launch() -> void {
  #if EPUB_LINUX_BUILD
    if (nextCtrl == Ctrl::NONE) return;
  #endif

  Ctrl theCtrl = nextCtrl;
  nextCtrl     = Ctrl::NONE;

  if (((theCtrl == Ctrl::LAST) && (lastCtrl[0] != currentCtrl)) || (theCtrl != currentCtrl)) {

    switch (currentCtrl) {
    case Ctrl::DIR:
      booksDirController.leave();
      break;
    case Ctrl::BOOK:
      bookController.leave();
      break;
    case Ctrl::PARAM:
      bookParamController.leave();
      break;
    case Ctrl::OPTION:
      optionController.leave();
      break;
    case Ctrl::TOC:
      tocController.leave();
      break;
    case Ctrl::NONE:
    case Ctrl::LAST:
      break;
    }

    Ctrl tmp    = currentCtrl;
    currentCtrl = (theCtrl == Ctrl::LAST) ? lastCtrl[0] : theCtrl;

    if (theCtrl == Ctrl::LAST) {
      for (int i = 1; i < LAST_COUNT; ++i) lastCtrl[i - 1] = lastCtrl[i];
      lastCtrl[LAST_COUNT - 1] = Ctrl::DIR;
    } else {
      for (int i = 1; i < LAST_COUNT; ++i) lastCtrl[i] = lastCtrl[i - 1];
      lastCtrl[0] = tmp;
    }

    switch (currentCtrl) {
    case Ctrl::DIR:
      booksDirController.enter();
      break;
    case Ctrl::BOOK:
      bookController.enter();
      break;
    case Ctrl::PARAM:
      bookParamController.enter();
      break;
    case Ctrl::OPTION:
      optionController.enter();
      break;
    case Ctrl::TOC:
      tocController.enter();
      break;
    case Ctrl::NONE:
    case Ctrl::LAST:
      break;
    }
  }
}

auto AppController::inputEvent(const EventMgr::Event &event) -> void {
  if (nextCtrl != Ctrl::NONE) launch();

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    if (event.kind == EventMgr::EventKind::PINCH_ENLARGE) {
      backLit.adjust(event.dist);
      return;
    } else if (event.kind == EventMgr::EventKind::PINCH_REDUCE) {
      backLit.adjust(-event.dist);
      return;
    } else if (event.kind == EventMgr::EventKind::WAKEUP_BUTTON) {
      CommonActions::powerItOff();
      return;
    }
  #endif

  switch (currentCtrl) {
  case Ctrl::DIR:
    booksDirController.inputEvent(event);
    break;
  case Ctrl::BOOK:
    bookController.inputEvent(event);
    break;
  case Ctrl::PARAM:
    bookParamController.inputEvent(event);
    break;
  case Ctrl::OPTION:
    optionController.inputEvent(event);
    break;
  case Ctrl::TOC:
    tocController.inputEvent(event);
    break;
  case Ctrl::NONE:
  case Ctrl::LAST:
    break;
  }
}

auto AppController::goingToDeepSleep() -> void {
  if (nextCtrl != Ctrl::NONE) launch();

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    backLit.turnOff();
    touch_screen.shutdown();
  #endif

  switch (currentCtrl) {
  case Ctrl::DIR:
    booksDirController.leave(true);
    break;
  case Ctrl::BOOK:
    bookController.leave(true);
    break;
  case Ctrl::PARAM:
    bookParamController.leave(true);
    break;
  case Ctrl::OPTION:
    optionController.leave(true);
    break;
  case Ctrl::TOC:
    tocController.leave(true);
    break;
  case Ctrl::NONE:
  case Ctrl::LAST:
    break;
  }
}
