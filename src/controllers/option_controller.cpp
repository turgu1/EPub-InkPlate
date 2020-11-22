// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __OPTION_CONTROLLER__ 1
#include "controllers/option_controller.hpp"

#include "controllers/common_actions.hpp"
#include "controllers/app_controller.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/form_viewer.hpp"
#include "models/books_dir.hpp"
#include "models/config.hpp"

#include "esp_system.h"

static FormViewer::Choice ok_cancel_choice[2] = {
  { "OK",     1 },
  { "CANCEL", 0 }
};

// static FormViewer::Choice yes_no_choice[2] = {
//   { "YES", 1 },
//   { "NO",  0 }
// };

static FormViewer::Choice timeout_choices[3] = {
  { "5",   5 },
  { "15", 15 },
  { "30", 30 }
};

static FormViewer::Choice battery_visual[4] = {
  { "NONE",    0 },
  { "PERCENT", 1 },
  { "VOLTAGE", 2 },
  { "ICON",    3 }
};

static FormViewer::Choice font_size_choices[4] = {
  { "8",       8 },
  { "10",     10 },
  { "12",     12 },
  { "15",     15 }
};

static FormViewer::Choice orientation_choices[3] = {
  { "LEFT",        0 },
  { "RIGHT",       1 },
  { "BOTTOM",      2 }
};

// static int8_t boolean_value;
static int8_t font_size;
static int8_t orientation;
static int8_t battery;
static int8_t timeout;
static int8_t ok;

static int8_t old_orientation;
static int8_t old_font_size;

static FormViewer::FormEntry form_entries[5] = {
  { "Minutes before sleeping :", &timeout,       3, timeout_choices,     FormViewer::HORIZONTAL_CHOICES },
  { "Battery Visualisation :",   &battery,       4, battery_visual,      FormViewer::VERTICAL_CHOICES   },
  { "Default Font Size (*):",    &font_size,     4, font_size_choices,   FormViewer::HORIZONTAL_CHOICES },
  { "Buttons Position (*):",     &orientation,   3, orientation_choices, FormViewer::VERTICAL_CHOICES   },
  { nullptr,                     &ok,            2, ok_cancel_choice,    FormViewer::HORIZONTAL_CHOICES }
};

extern bool start_web_server();
extern bool stop_web_server();

static void
parameters()
{
  config.get(Config::FONT_SIZE,   font_size  );
  config.get(Config::ORIENTATION, orientation);
  config.get(Config::BATTERY,     battery    );
  config.get(Config::TIMEOUT,     timeout    );

  old_orientation = orientation;
  old_font_size   = font_size;
  ok              = 0;

  form_viewer.show(form_entries, 5, 
    "(*) These items may trigger Books Pages Location Refresh.  "
    "Double-click on Select button to leave the form.");
  option_controller.set_form_is_shown();
}

static void
wifi_mode()
{
  event_mgr.set_stay_on(true); // DO NOT sleep

  if (start_web_server()) {
    option_controller.set_wait_for_key_after_wifi();
  }
}

static MenuViewer::MenuEntry menu[8] = {
  { MenuViewer::RETURN,   "Return to the e-books list",          CommonActions::return_to_last    },
  { MenuViewer::BOOK,     "Return to the last book being read",  CommonActions::show_last_book    },
  { MenuViewer::PARAMS,   "EPub-InkPlate parameters",            parameters                       },
  { MenuViewer::WIFI,     "WiFi Access to the e-books folder",   wifi_mode                        },
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
      if (ok) {
        config.put(Config::FONT_SIZE,   font_size  );
        config.put(Config::ORIENTATION, orientation);
        config.put(Config::BATTERY,     battery    );
        config.put(Config::TIMEOUT,     timeout    );
        config.save();

        if ((old_font_size != font_size) ||
            ((orientation != old_orientation) &&
             ((old_orientation == 2) ||
              (orientation     == 2)))) {
          int16_t dummy;
          books_dir.refresh(nullptr, dummy, true);
        }
      }
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