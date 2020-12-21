// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PARAM_CONTROLLER__ 1
#include "controllers/param_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/common_actions.hpp"
#include "models/books_dir.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/form_viewer.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE6_BUILD
  #include "esp_system.h"
  #include "eink.hpp"
  #include "esp.hpp"
  #include "soc/rtc.h"
#endif

static void 
books_list()
{
  app_controller.set_controller(AppController::Ctrl::DIR);
}

extern bool start_web_server();
extern bool stop_web_server();

static void
wifi_mode()
{
  #if EPUB_INKPLATE6_BUILD
    epub.close_file();
    fonts.clear();
    fonts.clear_glyph_caches();
    
    event_mgr.set_stay_on(true); // DO NOT sleep

    if (start_web_server()) {
      param_controller.set_wait_for_key_after_wifi();
    }
  #endif
}

static MenuViewer::MenuEntry menu[6] = {
  { MenuViewer::Icon::RETURN,    "Return to the e-books reader",        CommonActions::return_to_last    },
  { MenuViewer::Icon::BOOK_LIST, "E-Books list",                        books_list                       },
  { MenuViewer::Icon::WIFI,      "WiFi Access to the e-books folder",   wifi_mode                        },
  { MenuViewer::Icon::INFO,      "About the EPub-InkPlate application", CommonActions::about             },
  { MenuViewer::Icon::POWEROFF,  "Power OFF (Deep Sleep)",              CommonActions::power_off         },
  { MenuViewer::Icon::END_MENU,  nullptr,                               nullptr                          }
}; 

void 
ParamController::enter()
{
  menu_viewer.show(menu);
}

void 
ParamController::leave(bool going_to_deep_sleep)
{

}

void 
ParamController::key_event(EventMgr::KeyEvent key)
{
  if (form_is_shown) {
    if (form_viewer.event(key)) {
      form_is_shown = false;
    }
  }
  #if EPUB_INKPLATE6_BUILD
    else if (wait_for_key_after_wifi) {
      msg_viewer.show(MsgViewer::INFO, false, true, "Restarting", "The device is now restarting. Please wait.");
      wait_for_key_after_wifi = false;
      stop_web_server();
      esp_restart();
    }
  #endif
  else {
    if (menu_viewer.event(key)) {
      app_controller.set_controller(AppController::Ctrl::LAST);
    }
  }
}