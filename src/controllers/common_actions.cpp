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

#include "esp_system.h"
#include "esp_app_desc.h"

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

  app_controller.going_to_deep_sleep();

  if (event_mgr.staying_on()) {
    screen.force_full_update();
    msg_viewer.show(MsgViewer::MsgType::INFO, false, true, "Waiting for Power OFF",
      "Waiting for background tasks to complete before going to Deep Sleep mode.");
    while (event_mgr.staying_on()) {
      ESP::delay(5000);
    }
  }
  
  #if EPUB_INKPLATE_BUILD
    screen.force_full_update();
    msg_viewer.show(MsgViewer::MsgType::INFO, false, true, "Power OFF",
      "Entering Deep Sleep mode. " MSG);
    ESP::delay(1000);
    inkplate_platform.deep_sleep(INT_PIN, LEVEL);
  #else
    extern void exit_app();
    exit_app();
    exit(0);
  #endif
}

void
CommonActions::about()
{
  const esp_app_desc_t * descr = esp_app_get_description();

  menu_viewer.clear_highlight();
  msg_viewer.show(
    MsgViewer::MsgType::BOOK, 
    false,
    false,
    "About EPub-InkPlate", 
    "EPub EBook Reader Version [%s] for the InkPlate e-paper display devices. "
    "This application was made by Guy Turcotte, Quebec, QC, Canada, "
    "with great support from e-Radionica, and now from Soldered.",
    descr == nullptr ? APP_VERSION : descr->version);
}
