// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __COMMON_ACTIONS__ 1
#include "controllers/common_actions.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/book_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "controllers/event_mgr.hpp"
#include "models/books_dir.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/screen_saver.hpp"

#if EPUB_INKPLATE_BUILD
  #include "esp.hpp"
  #include "esp_app_desc.h"
  #include "esp_system.h"
  #include "inkplate_platform.hpp"

  #include "helpers/goto_deep_sleep.hpp"
#endif

auto CommonActions::returnToLast() -> void {
  appController.setController(AppController::Ctrl::LAST);
}

auto CommonActions::showLastBook() -> void { booksDirController.showLastBook(); }

auto CommonActions::refreshBooksDir() -> void {
  int16_t temp;

  booksDir.refresh(nullptr, temp, true);
  appController.setController(AppController::Ctrl::DIR);
}

auto CommonActions::powerItOff() -> void {

#if EPUB_INKPLATE_BUILD
  gotoDeepSleep(-1);
#else
  extern void exitApp();
  exitApp();
  exit(0);
#endif
}

auto CommonActions::about() -> void {
#if EPUB_INKPLATE_BUILD
  const esp_app_desc_t *descr = esp_app_get_description();

  // menu_viewer.clearHighlight();
  MsgViewer::show(MsgViewer::MsgType::BOOK, false, false, "About EPub-InkPlate",
                  "EPub EBook Reader Version [%s] for the InkPlate e-paper display devices. "
                  "This application was made by Guy Turcotte, Quebec, QC, Canada, "
                  "with great support from Soldered.",
                  descr == nullptr ? APP_VERSION : descr->version);
#else
  MsgViewer::show(MsgViewer::MsgType::BOOK, false, false, "About EPub-InkPlate",
                  "EPub EBook Reader Version [%s] for Linux. This application was made by Guy "
                  "Turcotte, Quebec, QC, Canada.",
                  APP_VERSION);
#endif
}
