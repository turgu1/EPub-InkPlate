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
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    #define MSG "Please press the WakeUp Button to restart the device."
    #define INT_PIN TouchScreen::INTERRUPT_PIN
    #define LEVEL 0
  #else
    #define MSG "Please press a key to restart the device."
    #define LEVEL 1
    #if EXTENDED_CASE
      #define INT_PIN PressKeys::INTERRUPT_PIN
    #else
      #define INT_PIN TouchKeys::INTERRUPT_PIN
    #endif
  #endif

  if (eventMgr.stayingOn()) {
    screen.forceFullUpdate();
    MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Waiting for Power OFF",
                    "Waiting for background tasks to complete before going to Deep Sleep mode.");
    while (eventMgr.stayingOn()) {
      #if EPUB_INKPLATE_BUILD
        ESP::delay(5000);
        if (pageLocs.isControlTaskReadyToBeStopped()) pageLocs.stopControlTask();
      #endif
    }
  }

  #if EPUB_INKPLATE_BUILD
    screen.forceFullUpdate();
    MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Power OFF",
                    "Entering Deep Sleep mode. " MSG);

    auto screen_saver = ScreenSaver::Make();
    screen_saver->show();

    ESP::delay(5000);

    appController.goingToDeepSleep();

    inkplate_platform.deep_sleep(INT_PIN, LEVEL);
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
