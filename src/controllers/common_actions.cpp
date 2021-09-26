// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __COMMON_ACTIONS__ 1
#include "controllers/common_actions.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "controllers/book_controller.hpp"
#include "controllers/event_mgr.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/menu_viewer.hpp"
#include "models/books_dir.hpp"

#if EPUB_INKPLATE_BUILD
  #include "inkplate_platform.hpp"
  #include "esp.hpp"
#endif

void
CommonActions::return_to_last()
{
  app_controller.set_controller(AppController::Ctrl::LAST);
}

void
CommonActions::show_last_book()
{
  books_dir_controller.show_last_book();
}

void
CommonActions::refresh_books_dir()
{
  int16_t temp;

  books_dir.refresh(nullptr, temp, true);
  app_controller.set_controller(AppController::Ctrl::DIR);
}

void
CommonActions::power_it_off()
{
  #if EXTENDED_CASE
    #define INT_PIN PressKeys::INTERRUPT_PIN
  #else
    #define INT_PIN TouchKeys::INTERRUPT_PIN
  #endif

  app_controller.going_to_deep_sleep();
  #if EPUB_INKPLATE_BUILD
    msg_viewer.show(MsgViewer::INFO, false, true, "Power OFF",
      "Entering Deep Sleep mode. Please press a key to restart the device.");
    ESP::delay(500);
    inkplate_platform.deep_sleep(INT_PIN, 1);
  #else
    extern void exit_app();
    exit_app();
    exit(0);
  #endif
}

void
CommonActions::about()
{
  menu_viewer.clear_highlight();
  msg_viewer.show(
    MsgViewer::BOOK, 
    false,
    false,
    "About EPub-InkPlate", 
    "EPub EBook Reader Version %s for the InkPlate e-paper display devices. "
    "This application was made by Guy Turcotte, Quebec, QC, Canada, "
    "with great support from e-Radionica.",
    APP_VERSION);
}

#if EPUB_LINUX_BUILD && DEBUGGING
  void
  CommonActions::debugging()
  {
    menu_viewer.clear_highlight();
  }
#endif
