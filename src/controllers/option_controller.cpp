// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __OPTION_CONTROLLER__ 1
#include "controllers/option_controller.hpp"

#include "controllers/common_actions.hpp"
#include "controllers/app_controller.hpp"
#include "controllers/books_dir_controller.hpp"
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

static Screen::Orientation     orientation;
static Screen::PixelResolution  resolution;
static int8_t show_battery;
static int8_t timeout;
static int8_t show_images;
static int8_t font_size;
static int8_t use_fonts_in_books;
static int8_t default_font;
static int8_t show_heap;
static int8_t show_title;
static int8_t dir_view;
static int8_t done;

static Screen::Orientation     old_orientation;
static Screen::PixelResolution  old_resolution;
static int8_t old_show_images;
static int8_t old_font_size;
static int8_t old_use_fonts_in_books;
static int8_t old_default_font;
static int8_t old_show_title;
static int8_t old_dir_view;

#if INKPLATE_6PLUS || TOUCH_TRIAL
  static constexpr int8_t MAIN_FORM_SIZE = 8;
#else
  static constexpr int8_t MAIN_FORM_SIZE = 7;
#endif
static FormViewer::FormEntry main_params_form_entries[MAIN_FORM_SIZE] = {
  { "Minutes Before Sleeping :",  &timeout,                3, FormViewer::timeout_choices,     FormViewer::FormEntryType::HORIZONTAL  },
  { "Books Directory View :",     &dir_view,               2, FormViewer::dir_view_choices,    FormViewer::FormEntryType::HORIZONTAL  },
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    { "uSDCard Position (*):", (int8_t *) &orientation, 4, FormViewer::orientation_choices, FormViewer::FormEntryType::VERTICAL },
  #else
    { "Buttons Position (*):",    (int8_t *) &orientation, 3, FormViewer::orientation_choices, FormViewer::FormEntryType::VERTICAL    },
  #endif
  { "Pixel Resolution :",         (int8_t *) &resolution,  2, FormViewer::resolution_choices,  FormViewer::FormEntryType::HORIZONTAL  },
  { "Show Battery Level :",       &show_battery,           4, FormViewer::battery_visual,      FormViewer::FormEntryType::VERTICAL    },
  { "Show Title (*):",            &show_title,             2, FormViewer::yes_no_choices,      FormViewer::FormEntryType::HORIZONTAL  },
  { "Show Heap Sizes :",          &show_heap,              2, FormViewer::yes_no_choices,      FormViewer::FormEntryType::HORIZONTAL  },
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    { nullptr,                    &done,                   1, FormViewer::done_choices,        FormViewer::FormEntryType::DONE        }
  #endif
};

#if INKPLATE_6PLUS || TOUCH_TRIAL
  static constexpr int8_t FONT_FORM_SIZE = 5;
#else
  static constexpr int8_t FONT_FORM_SIZE = 4;
#endif
static FormViewer::FormEntry font_params_form_entries[FONT_FORM_SIZE] = {
  { "Default Font Size (*):",      &font_size,          4, FormViewer::font_size_choices, FormViewer::FormEntryType::HORIZONTAL },
  { "Use Fonts in E-books (*):",   &use_fonts_in_books, 2, FormViewer::yes_no_choices,    FormViewer::FormEntryType::HORIZONTAL },
  { "Default Font (*):",           &default_font,       8, FormViewer::font_choices,      FormViewer::FormEntryType::VERTICAL   },
  { "Show Images in E-books (*):", &show_images,        2, FormViewer::yes_no_choices,    FormViewer::FormEntryType::HORIZONTAL },
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    { nullptr,                     &done,               1, FormViewer::done_choices,      FormViewer::FormEntryType::DONE       }
  #endif
};

extern bool start_web_server();
extern bool  stop_web_server();

static void
main_parameters()
{
  config.get(Config::Ident::ORIENTATION,      (int8_t *) &orientation);
  config.get(Config::Ident::DIR_VIEW,         &dir_view              );
  config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *) &resolution );
  config.get(Config::Ident::BATTERY,          &show_battery          );
  config.get(Config::Ident::SHOW_TITLE,       &show_title            );
  config.get(Config::Ident::SHOW_HEAP,        &show_heap             );
  config.get(Config::Ident::TIMEOUT,          &timeout               );

  old_orientation = orientation;
  old_dir_view    = dir_view;
  old_resolution  = resolution;
  old_show_title  = show_title;
  done            = 1;

  form_viewer.show(
    main_params_form_entries, 
    MAIN_FORM_SIZE, 
    "(*) Will trigger e-book pages location recalc.");

  option_controller.set_main_form_is_shown();
}

static void
default_parameters()
{
  config.get(Config::Ident::SHOW_IMAGES,        &show_images       );
  config.get(Config::Ident::FONT_SIZE,          &font_size         );
  config.get(Config::Ident::USE_FONTS_IN_BOOKS, &use_fonts_in_books);
  config.get(Config::Ident::DEFAULT_FONT,       &default_font      );
  
  old_show_images        = show_images;
  old_use_fonts_in_books = use_fonts_in_books;
  old_default_font       = default_font;
  old_font_size          = font_size;
  done                   = 1;

  form_viewer.show(
    font_params_form_entries, 
    FONT_FORM_SIZE, 
    "(*) Used as e-book default values.");

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

void calibrate()
{
  event_mgr.show_calibration();
  option_controller.set_calibration_is_shown();
}

// IMPORTANT!!!
// The first (menu[0]) and the last menu entry (the one before END_MENU) MUST ALWAYS BE VISIBLE!!!

static MenuViewer::MenuEntry menu[] = {

  { MenuViewer::Icon::RETURN,      "Return to the e-books list",           CommonActions::return_to_last    , true,  true  },
  { MenuViewer::Icon::BOOK,        "Return to the last e-book being read", CommonActions::show_last_book    , true,  true  },
  { MenuViewer::Icon::MAIN_PARAMS, "Main parameters",                      main_parameters                  , true,  true  },
  { MenuViewer::Icon::FONT_PARAMS, "Default e-books parameters",           default_parameters               , true,  true  },
  { MenuViewer::Icon::WIFI,        "WiFi Access to the e-books folder",    wifi_mode                        , true,  true  },
  { MenuViewer::Icon::REFRESH,     "Refresh the e-books list",             CommonActions::refresh_books_dir , true,  true  },
  #if INKPLATE_6PLUS
    { MenuViewer::Icon::CALIB,     "Touch Screen Calibration",             calibrate                        , true,  false },
  #endif
  #if EPUB_LINUX_BUILD && DEBUGGING
    { MenuViewer::Icon::DEBUG,     "Debugging",                            CommonActions::debugging         , true,  true  },
  #endif
  { MenuViewer::Icon::INFO,        "About the EPub-InkPlate application",  CommonActions::about             , true,  true  },
  { MenuViewer::Icon::POWEROFF,    "Power OFF (Deep Sleep)",               CommonActions::power_it_off      , true,  true  },
  { MenuViewer::Icon::END_MENU,     nullptr,                               nullptr                          , false, false }
};

void
OptionController::set_font_count(uint8_t count)
{
  font_params_form_entries[2].choice_count = count;
}

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
OptionController::input_event(const EventMgr::Event & event)
{
  if (main_form_is_shown) {
    if (form_viewer.event(event)) {
      main_form_is_shown = false;
      // if (ok) {
        config.put(Config::Ident::ORIENTATION,      (int8_t) orientation);
        config.put(Config::Ident::DIR_VIEW,         dir_view            );
        config.put(Config::Ident::PIXEL_RESOLUTION, (int8_t) resolution );
        config.put(Config::Ident::BATTERY,          show_battery        );
        config.put(Config::Ident::SHOW_TITLE,       show_title          );
        config.put(Config::Ident::SHOW_HEAP,        show_heap           );
        config.put(Config::Ident::TIMEOUT,          timeout             );
        config.save();

        if (old_orientation != orientation) {
          screen.set_orientation(orientation);
          event_mgr.set_orientation(orientation);
          books_dir_controller.new_orientation();
        }

        if (old_dir_view != dir_view) {

        }

        if (old_resolution != resolution) {
          fonts.clear_glyph_caches();
          screen.set_pixel_resolution(resolution);
        }

        if ((old_orientation != orientation) ||
            (old_show_title  != show_title )) {
          epub.update_book_format_params();
        }

        if ((old_orientation != orientation) || 
            (old_resolution  != resolution )) {
          menu_viewer.show(menu, 2, true);
        }
        else {
          menu_viewer.clear_highlight();
        }
      // }
    }
  }
  else if (calibration_is_shown) {
    if (event_mgr.calibration_event(event)) {
      calibration_is_shown = false;

      const EventMgr::CalibData & calib_data = event_mgr.get_calib_data();

      LOG_I("Calibration data: [%u, %u] [%u, %u]",
        calib_data.x[0],
        calib_data.y[0],
        calib_data.x[1],
        calib_data.y[1]
      );
      menu_viewer.show(menu, 0, true);
    }
  }
  else if (font_form_is_shown) {
    if (form_viewer.event(event)) {
      font_form_is_shown = false;
      // if (ok) {
        config.put(Config::Ident::SHOW_IMAGES,        show_images       );
        config.put(Config::Ident::FONT_SIZE,          font_size         );
        config.put(Config::Ident::DEFAULT_FONT,       default_font      );
        config.put(Config::Ident::USE_FONTS_IN_BOOKS, use_fonts_in_books);
        config.save();

        if ((old_show_images        != show_images       ) ||
            (old_font_size          != font_size         ) ||
            (old_default_font       != default_font      ) ||
            (old_use_fonts_in_books != use_fonts_in_books)) {
          epub.update_book_format_params();  
        }

        if (old_default_font != default_font) {
          fonts.adjust_default_font(default_font);
        }

        if (old_use_fonts_in_books != use_fonts_in_books) {
          if (use_fonts_in_books == 0) {
            fonts.clear();
            fonts.clear_glyph_caches();
          }
        }
      // }
      menu_viewer.clear_highlight();
    }
  }
  #if EPUB_INKPLATE_BUILD
    else if (wait_for_key_after_wifi) {
      msg_viewer.show(MsgViewer::INFO, 
                      false, true, 
                      "Restarting", 
                      "The device is now restarting. Please wait.");
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
    if (menu_viewer.event(event)) {
      if (books_refresh_needed) {
        books_refresh_needed = false;
        int16_t dummy;
        books_dir.refresh(nullptr, dummy, true);
      }
      app_controller.set_controller(AppController::Ctrl::LAST);
    }
  }
}
