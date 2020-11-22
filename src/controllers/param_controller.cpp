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

#include "esp_system.h"

static void 
books_list()
{
  app_controller.set_controller(AppController::DIR);
}

extern bool start_web_server();
extern bool stop_web_server();

static void
wifi_mode()
{
  event_mgr.set_stay_on(true); // DO NOT sleep

  if (start_web_server()) {
    param_controller.set_wait_for_key_after_wifi();
  }
}

// static void
// parameters()
// {

// }

static MenuViewer::MenuEntry menu[6] = {
  { MenuViewer::RETURN,    "Return to the e-books reader",        CommonActions::return_to_last    },
  { MenuViewer::BOOK_LIST, "Books list",                          books_list                       },
  { MenuViewer::WIFI,      "WiFi Access to the e-books folder",   wifi_mode                        },
  { MenuViewer::INFO,      "About the EPub-InkPlate application", CommonActions::about             },
  { MenuViewer::POWEROFF,  "Power OFF (Deep Sleep)",              CommonActions::power_off         },
  { MenuViewer::END_MENU,  nullptr,                               nullptr                          }
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
  else if (wait_for_key_after_wifi) {
    msg_viewer.show(MsgViewer::INFO, false, true, "Restarting", "The device is now restarting. Please wait.");
    wait_for_key_after_wifi = false;
    stop_web_server();
    esp_restart();
  }
  else {
    menu_viewer.event(key);
  }
}