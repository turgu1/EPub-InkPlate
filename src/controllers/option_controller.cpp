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

#if EPUB_INKPLATE6_BUILD
  #include "esp_system.h"
#endif

static FormViewer::Choice ok_cancel_choices[2] = {
  { "OK",     1 },
  { "CANCEL", 0 }
};

static FormViewer::Choice yes_no_choices[2] = {
  { "YES", 1 },
  { "NO",  0 }
};

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
  { "8",   8 },
  { "10", 10 },
  { "12", 12 },
  { "15", 15 }
};

static FormViewer::Choice orientation_choices[3] = {
  { "LEFT",   0 },
  { "RIGHT",  1 },
  { "BOTTOM", 2 }
};

static FormViewer::Choice font_choices[8] = {
  { "Caladea S",     0 },
  { "Crimson S",     1 },
  { "Asap",          2 },
  { "Asap Cond",     3 },
  { "DejaVu S",      4 },
  { "DejaVu Cond S", 5 },
  { "DejaVu",        6 },
  { "DejaVu Cond",   7 }
};

// static int8_t boolean_value;
static int8_t font_size;
static Screen::Orientation orientation;
static int8_t battery;
static int8_t timeout;
static int8_t show_images;
static int8_t use_fonts_in_books;
static int8_t default_font;
static int8_t ok;

static Screen::Orientation old_orientation;
static int8_t old_font_size;
static int8_t old_show_images;
static int8_t old_use_fonts_in_books;
static int8_t old_default_font;

static FormViewer::FormEntry main_params_form_entries[5] = {
  { "Minutes before sleeping :", &timeout,                3, timeout_choices,     FormViewer::HORIZONTAL_CHOICES },
  { "Battery Visualisation :",   &battery,                4, battery_visual,      FormViewer::VERTICAL_CHOICES   },
  { "Buttons Position (*):",     (int8_t *) &orientation, 3, orientation_choices, FormViewer::VERTICAL_CHOICES   },
  { "Show Images in books (*):", &show_images,            2, yes_no_choices,      FormViewer::HORIZONTAL_CHOICES },
  { nullptr,                     &ok,                     2, ok_cancel_choices,   FormViewer::HORIZONTAL_CHOICES }
};

static FormViewer::FormEntry font_params_form_entries[4] = {
  { "Default Font Size (*):",    &font_size,              4, font_size_choices,   FormViewer::HORIZONTAL_CHOICES },
  { "Use fonts in books (*):",   &use_fonts_in_books,     2, yes_no_choices,      FormViewer::HORIZONTAL_CHOICES },
  { "Default font (*):",         &default_font,           8, font_choices,        FormViewer::VERTICAL_CHOICES   },
  { nullptr,                     &ok,                     2, ok_cancel_choices,   FormViewer::HORIZONTAL_CHOICES }
};

extern bool start_web_server();
extern bool stop_web_server();

static void
main_parameters()
{
  config.get(Config::ORIENTATION, (int8_t *) &orientation);
  config.get(Config::BATTERY,     &battery               );
  config.get(Config::TIMEOUT,     &timeout               );
  config.get(Config::SHOW_IMAGES, &show_images           );

  old_orientation = orientation;
  old_show_images = show_images;

  ok              = 0;

  form_viewer.show(main_params_form_entries, 5, 
    "(*) These items may trigger Books Pages Location Refresh.  "
    "Double-click on Select button to leave the form.");

  option_controller.set_main_form_is_shown();
}

static void
font_parameters()
{
  config.get(Config::FONT_SIZE,          &font_size             );
  config.get(Config::USE_FONTS_IN_BOOKS, &use_fonts_in_books    );
  config.get(Config::DEFAULT_FONT,       &default_font          );
  
  old_use_fonts_in_books = use_fonts_in_books;
  old_default_font       = default_font;
  old_font_size          = font_size;

  ok                     = 0;

  form_viewer.show(font_params_form_entries, 4, 
    "(*) These items may trigger Books Pages Location Refresh.  "
    "Double-click on Select button to leave the form.");

  option_controller.set_font_form_is_shown();
}

static void
wifi_mode()
{
  #if EPUB_INKPLATE6_BUILD  
    event_mgr.set_stay_on(true); // DO NOT sleep

    if (start_web_server()) {
      option_controller.set_wait_for_key_after_wifi();
    }
  #endif
}

static MenuViewer::MenuEntry menu[9] = {
  { MenuViewer::RETURN,      "Return to the e-books list",          CommonActions::return_to_last    },
  { MenuViewer::BOOK,        "Return to the last book being read",  CommonActions::show_last_book    },
  { MenuViewer::MAIN_PARAMS, "Main parameters",                     main_parameters                  },
  { MenuViewer::FONT_PARAMS, "Font parameters",                     font_parameters                  },
  { MenuViewer::WIFI,        "WiFi Access to the e-books folder",   wifi_mode                        },
  { MenuViewer::REFRESH,     "Refresh the e-books list",            CommonActions::refresh_books_dir },
  { MenuViewer::INFO,        "About the EPub-InkPlate application", CommonActions::about             },
  { MenuViewer::POWEROFF,    "Power OFF (Deep Sleep)",              CommonActions::power_off         },
  { MenuViewer::END_MENU,    nullptr,                               nullptr                          }
};

void 
OptionController::enter()
{
  menu_viewer.show(menu);
  main_form_is_shown = false;
  font_form_is_shown = false;
}

void 
OptionController::leave(bool going_to_deep_sleep)
{
}

void 
OptionController::key_event(EventMgr::KeyEvent key)
{
  if (main_form_is_shown) {
    if (form_viewer.event(key)) {
      main_form_is_shown = false;
      if (ok) {
        config.put(Config::SHOW_IMAGES, show_images         );
        config.put(Config::ORIENTATION, (int8_t) orientation);
        config.put(Config::BATTERY,     battery             );
        config.put(Config::TIMEOUT,     timeout             );
        config.save();

        if (old_orientation != orientation) {
          screen.set_orientation(orientation);
          event_mgr.set_orientation(orientation);
        }

        if ((old_show_images != show_images) ||
            ((orientation != old_orientation) &&
             ((old_orientation == Screen::O_BOTTOM) ||
              (orientation     == Screen::O_BOTTOM)))) {
          books_refresh_needed = true;  
        }
      }
    }
  }
  else if (font_form_is_shown) {
    if (form_viewer.event(key)) {
      font_form_is_shown = false;
      if (ok) {
        config.put(Config::FONT_SIZE,          font_size         );
        config.put(Config::DEFAULT_FONT,       default_font      );
        config.put(Config::USE_FONTS_IN_BOOKS, use_fonts_in_books);
        config.save();

        if ((old_font_size          != font_size   ) ||
            (old_default_font       != default_font) ||
            (old_use_fonts_in_books != use_fonts_in_books)) {
          books_refresh_needed = true;  
        }
        if (old_default_font != default_font) {
          fonts.setup();
        }
      }
    }
  }
  #if EPUB_INKPLATE6_BUILD
    else if (wait_for_key_after_wifi) {
      msg_viewer.show(MsgViewer::INFO, false, true, "Restarting", "The device is now restarting. Please wait.");
      wait_for_key_after_wifi = false;
      stop_web_server();
      if (books_refresh_needed) {
        books_refresh_needed = false;
        int16_t dummy;
        books_dir.refresh(nullptr, dummy, true);
      }
      esp_restart();
    }
  #endif
  else {
    if (menu_viewer.event(key)) {
      if (books_refresh_needed) {
        books_refresh_needed = false;
        int16_t dummy;
        books_dir.refresh(nullptr, dummy, true);
      }
      app_controller.set_controller(AppController::LAST);
    }
  }
}