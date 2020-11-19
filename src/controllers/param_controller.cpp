// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PARAM_CONTROLLER__ 1
#include "controllers/param_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/common_actions.hpp"
#include "viewers/menu_viewer.hpp"

static void 
books_list()
{
  app_controller.set_controller(AppController::DIR);
}

static void
parameters()
{

}

static MenuViewer::MenuEntry menu[8] = {
  { MenuViewer::RETURN,    "Return to the e-books reader",        CommonActions::return_to_last    },
  { MenuViewer::BOOK_LIST, "Books list",                          books_list                       },
  { MenuViewer::PARAMS,    "EBook viewing parameters",            parameters                       },
  { MenuViewer::WIFI,      "WiFi Access to the e-books folder",   CommonActions::wifi_mode         },
  { MenuViewer::REFRESH,   "Refresh the e-books list",            CommonActions::refresh_books_dir },
  { MenuViewer::INFO,      "About the EPub-InkPlate application", CommonActions::about             },
  { MenuViewer::POWEROFF,  "Power OFF (Deep Sleep)",              CommonActions::power_off         },
  { MenuViewer::END_MENU,  nullptr,                               nullptr                          }
}; 

ParamController::ParamController()
{

}

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
  menu_viewer.event(key);  
}