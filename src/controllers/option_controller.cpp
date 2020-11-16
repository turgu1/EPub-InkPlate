// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __OPTION_CONTROLLER__ 1
#include "controllers/option_controller.hpp"

#include "controllers/app_controller.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "models/books_dir.hpp"

static void
return_action()
{
  app_controller.set_controller(AppController::LAST);
}

static void
refresh_action()
{
  books_dir.refresh();
}

static void
power_off()
{

}

static void
parameters()
{

}

static void
wifi_mode()
{
  MsgViewer::show_progress_bar("Petit test %d livres...", 31);
  MsgViewer::set_progress_bar(100);
}

static void
about()
{
  MsgViewer::show(
    MsgViewer::BOOK, 
    false,
    "About EPub-InkPlate", 
    "EBook Reader Version %s for the InkPlate-6 e-paper display. "
    "Made by Guy Turcotte, Quebec, Canada. "
    "With great support from E-Radionica.",
    APP_VERSION);
}

MenuViewer::MenuEntry menu[7] = {
  { MenuViewer::RETURN,   "Return to the e-books list",          return_action  },
  { MenuViewer::PARAMS,   "EPub-InkPlate parameters",            parameters     },
  { MenuViewer::WIFI,     "WiFi Access to the e-books folder",   wifi_mode      },
  { MenuViewer::REFRESH,  "Refresh the e-books list",            refresh_action },
  { MenuViewer::INFO,     "About the EPub-InkPlate application", about          },
  { MenuViewer::POWEROFF, "Power OFF (Deep Sleep)",              power_off      },
  { MenuViewer::END_MENU,  nullptr,                              nullptr        }
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