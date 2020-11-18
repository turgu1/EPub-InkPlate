// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __OPTION_CONTROLLER__ 1
#include "controllers/option_controller.hpp"

#include "controllers/common_actions.hpp"
#include "viewers/menu_viewer.hpp"

static void
parameters()
{

}

static MenuViewer::MenuEntry menu[7] = {
  { MenuViewer::RETURN,   "Return to the e-books list",          CommonActions::return_to_last    },
  { MenuViewer::PARAMS,   "EPub-InkPlate parameters",            parameters                       },
  { MenuViewer::WIFI,     "WiFi Access to the e-books folder",   CommonActions::wifi_mode         },
  { MenuViewer::REFRESH,  "Refresh the e-books list",            CommonActions::refresh_books_dir },
  { MenuViewer::INFO,     "About the EPub-InkPlate application", CommonActions::about             },
  { MenuViewer::POWEROFF, "Power OFF (Deep Sleep)",              CommonActions::power_off         },
  { MenuViewer::END_MENU,  nullptr,                              nullptr                          }
};

OptionController::OptionController()
{
  
}

void 
OptionController::enter()
{
  menu_viewer.show(menu);
}

void 
OptionController::leave()
{

}

void 
OptionController::key_event(EventMgr::KeyEvent key)
{
  menu_viewer.event(key);
}