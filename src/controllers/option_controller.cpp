// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __OPTION_CONTROLLER__ 1
#include "controllers/option_controller.hpp"

#include "controllers/common_actions.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/form_viewer.hpp"

static FormViewer::Choice ok_cancel_choice[2] = {
  { "OK",     1 },
  { "CANCEL", 0 }
};

static FormViewer::Choice yes_no_choice[2] = {
  { "YES", 1 },
  { "NO",  0 }
};

static FormViewer::Choice battery_visual[3] = {
  { "NONE",    0 },
  { "PERCENT", 1 },
  { "ICON",    2 }
};

static FormViewer::Choice font_size_choices[4] = {
  { "SMALL",       0 },
  { "MEDIUM",      1 },
  { "LARGE",       2 },
  { "VERY LARGE",  3 }
};

static FormViewer::Choice orientation_choices[3] = {
  { "LEFT",        0 },
  { "RIGHT",       1 },
  { "BOTTOM",      2 }
};

static int8_t boolean_value;
static int8_t font_size;
static int8_t orientation;
static int8_t battery_level;
static int8_t ok;

static FormViewer::FormEntry form_entries[5] = {
  { "Boolean Test :",        &boolean_value, 2, yes_no_choice,       FormViewer::HORIZONTAL_CHOICES },
  { "Default Font Size :",   &font_size,     4, font_size_choices,   FormViewer::VERTICAL_CHOICES   },
  { "Screen Orientation :",  &orientation,   3, orientation_choices, FormViewer::VERTICAL_CHOICES   },
  { "Battery Visualisation", &battery_level, 3, battery_visual,      FormViewer::VERTICAL_CHOICES   },
  { nullptr,                 &ok,            2, ok_cancel_choice,    FormViewer::HORIZONTAL_CHOICES }
};

static void
parameters()
{
  form_viewer.show(form_entries, 5);
  option_controller.set_form_is_shown();
}

static MenuViewer::MenuEntry menu[8] = {
  { MenuViewer::RETURN,   "Return to the e-books list",          CommonActions::return_to_last    },
  { MenuViewer::BOOK,     "Return to the last book being read",  CommonActions::show_last_book    },
  { MenuViewer::PARAMS,   "EPub-InkPlate parameters",            parameters                       },
  { MenuViewer::WIFI,     "WiFi Access to the e-books folder",   CommonActions::wifi_mode         },
  { MenuViewer::REFRESH,  "Refresh the e-books list",            CommonActions::refresh_books_dir },
  { MenuViewer::INFO,     "About the EPub-InkPlate application", CommonActions::about             },
  { MenuViewer::POWEROFF, "Power OFF (Deep Sleep)",              CommonActions::power_off         },
  { MenuViewer::END_MENU,  nullptr,                              nullptr                          }
};

void 
OptionController::enter()
{
  menu_viewer.show(menu);
  form_is_shown = false;
}

void 
OptionController::leave(bool going_to_deep_sleep)
{
}

void 
OptionController::key_event(EventMgr::KeyEvent key)
{
  if (form_is_shown) {
    if (form_viewer.event(key)) {
      form_is_shown = false;
    }
  }
  else {
    menu_viewer.event(key);
  }
}