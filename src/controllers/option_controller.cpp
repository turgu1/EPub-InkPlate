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
#include "models/epub.hpp"

#if EPUB_INKPLATE_BUILD
  #include "esp_system.h"
#endif


// static int8_t boolean_value;

static Screen::Orientation    orientation;
static Screen::PixelResolution resolution;
static int8_t show_battery;
static int8_t timeout;
static int8_t show_images;
static int8_t font_size;
static int8_t use_fonts_in_books;
static int8_t default_font;
static int8_t ok;

static Screen::Orientation    old_orientation;
static Screen::PixelResolution old_resolution;
static int8_t old_show_images;
static int8_t old_font_size;
static int8_t old_use_fonts_in_books;
static int8_t old_default_font;

static constexpr int8_t MAIN_FORM_SIZE = 6;
static FormViewer::FormEntry main_params_form_entries[MAIN_FORM_SIZE] = {
  { "Minutes before sleeping :", &timeout,                3, FormViewer::timeout_choices,     FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Buttons Position:",         (int8_t *) &orientation, 3, FormViewer::orientation_choices, FormViewer::FormEntryType::VERTICAL_CHOICES   },
  { "Show Images in books (*):", &show_images,            2, FormViewer::yes_no_choices,      FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Pixel Resolution :",        (int8_t *) &resolution,  2, FormViewer::resolution_choices,  FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Battery Visualisation :",   &show_battery,           4, FormViewer::battery_visual,      FormViewer::FormEntryType::VERTICAL_CHOICES   },
  { nullptr,                     &ok,                     2, FormViewer::ok_cancel_choices,   FormViewer::FormEntryType::HORIZONTAL_CHOICES }
};

static constexpr int8_t FONT_FORM_SIZE = 4;
static FormViewer::FormEntry font_params_form_entries[FONT_FORM_SIZE] = {
  { "Default Font Size (*):",  &font_size,          4, FormViewer::font_size_choices, FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Use fonts in books (*):", &use_fonts_in_books, 2, FormViewer::yes_no_choices,    FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Default font (*):",       &default_font,       8, FormViewer::font_choices,      FormViewer::FormEntryType::VERTICAL_CHOICES   },
  { nullptr,                   &ok,                 2, FormViewer::ok_cancel_choices, FormViewer::FormEntryType::HORIZONTAL_CHOICES }
};

extern bool start_web_server();
extern bool  stop_web_server();

static void
main_parameters()
{
  config.get(Config::Ident::ORIENTATION,      (int8_t *) &orientation);
  config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *) &resolution );
  config.get(Config::Ident::BATTERY,          &show_battery          );
  config.get(Config::Ident::TIMEOUT,          &timeout               );
  config.get(Config::Ident::SHOW_IMAGES,      &show_images           );

  old_orientation = orientation;
  old_show_images = show_images;
  old_resolution  = resolution;
  ok              = 0;

  form_viewer.show(
    main_params_form_entries, 
    MAIN_FORM_SIZE, 
    "(*) These items used as e-book default values.");

  option_controller.set_main_form_is_shown();
}

static void
font_parameters()
{
  config.get(Config::Ident::FONT_SIZE,          &font_size         );
  config.get(Config::Ident::USE_FONTS_IN_BOOKS, &use_fonts_in_books);
  config.get(Config::Ident::DEFAULT_FONT,       &default_font      );
  
  old_use_fonts_in_books = use_fonts_in_books;
  old_default_font       = default_font;
  old_font_size          = font_size;
  ok                     = 0;

  form_viewer.show(
    font_params_form_entries, 
    FONT_FORM_SIZE, 
    "(*) These items used as e-book default values.");

  option_controller.set_font_form_is_shown();
}

static void
wifi_mode()
{
  #if EPUB_INKPLATE_BUILD  
    epub.close_file();
    fonts.clear();
    fonts.clear_glyph_caches();
    
    event_mgr.set_stay_on(true); // DO NOT sleep

    if (start_web_server()) {
      option_controller.set_wait_for_key_after_wifi();
    }
  #endif
}

static MenuViewer::MenuEntry menu[9] = {
  { MenuViewer::Icon::RETURN,      "Return to the e-books list",          CommonActions::return_to_last    },
  { MenuViewer::Icon::BOOK,        "Return to the last book being read",  CommonActions::show_last_book    },
  { MenuViewer::Icon::MAIN_PARAMS, "Main parameters",                     main_parameters                  },
  { MenuViewer::Icon::FONT_PARAMS, "Font parameters",                     font_parameters                  },
  { MenuViewer::Icon::WIFI,        "WiFi Access to the e-books folder",   wifi_mode                        },
  { MenuViewer::Icon::REFRESH,     "Refresh the e-books list",            CommonActions::refresh_books_dir },
  { MenuViewer::Icon::INFO,        "About the EPub-InkPlate application", CommonActions::about             },
  { MenuViewer::Icon::POWEROFF,    "Power OFF (Deep Sleep)",              CommonActions::power_off         },
  { MenuViewer::Icon::END_MENU,    nullptr,                               nullptr                          }
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
        config.put(Config::Ident::SHOW_IMAGES,      show_images         );
        config.put(Config::Ident::ORIENTATION,      (int8_t) orientation);
        config.put(Config::Ident::PIXEL_RESOLUTION, (int8_t) resolution );
        config.put(Config::Ident::BATTERY,          show_battery        );
        config.put(Config::Ident::TIMEOUT,          timeout             );
        config.save();

        if (old_orientation != orientation) {
          screen.set_orientation(orientation);
          event_mgr.set_orientation(orientation);
        }

        if (old_resolution != resolution) {
          fonts.clear_glyph_caches();
          screen.set_pixel_resolution(resolution);
        }

        // if ((old_show_images != show_images) ||
        //     ((old_orientation != orientation) &&
        //      ((old_orientation == Screen::Orientation::BOTTOM) ||
        //       (orientation     == Screen::Orientation::BOTTOM)))) {
        //   books_refresh_needed = true;  
        // }
      }
    }
  }
  else if (font_form_is_shown) {
    if (form_viewer.event(key)) {
      font_form_is_shown = false;
      if (ok) {
        config.put(Config::Ident::FONT_SIZE,          font_size         );
        config.put(Config::Ident::DEFAULT_FONT,       default_font      );
        config.put(Config::Ident::USE_FONTS_IN_BOOKS, use_fonts_in_books);
        config.save();

        // if ((old_font_size          != font_size   ) ||
        //     (old_default_font       != default_font) ||
        //     (old_use_fonts_in_books != use_fonts_in_books)) {
        //   books_refresh_needed = true;  
        // }
        if (old_default_font != default_font) {
          fonts.setup();
        }
      }
    }
  }
  #if EPUB_INKPLATE_BUILD
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
      app_controller.set_controller(AppController::Ctrl::LAST);
    }
  }
}