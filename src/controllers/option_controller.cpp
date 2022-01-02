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
#include "models/nvs_mgr.hpp"

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

#if DATE_TIME_RTC
  static int8_t show_heap_or_rtc;
  static uint16_t year;
  static uint16_t month, day, hour, minute, second;
#else
  static int8_t show_heap;
#endif

#if INKPLATE_6PLUS || TOUCH_TRIAL
  static constexpr int8_t MAIN_FORM_SIZE = 8;
#else
  static constexpr int8_t MAIN_FORM_SIZE = 7;
#endif

static FormEntry main_params_form_entries[MAIN_FORM_SIZE] = {
  { .caption = "Minutes Before Sleeping :",  .u = { .ch = { .value = &timeout,                .choice_count = 3, .choices = FormChoiceField::timeout_choices        } }, .entry_type = FormEntryType::HORIZONTAL  },
  { .caption = "Books Directory View :",     .u = { .ch = { .value = &dir_view,               .choice_count = 2, .choices = FormChoiceField::dir_view_choices       } }, .entry_type = FormEntryType::HORIZONTAL  },
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    { .caption = "uSDCard Position (*):",    .u = { .ch = { .value = (int8_t *) &orientation, .choice_count = 4, .choices = FormChoiceField::orientation_choices    } }, .entry_type = FormEntryType::VERTICAL    },
  #else
    { .caption = "Buttons Position (*):",    .u = { .ch = { .value = (int8_t *) &orientation, .choice_count = 3, .choices = FormChoiceField::orientation_choices    } }, .entry_type = FormEntryType::VERTICAL    },
  #endif
  { .caption = "Pixel Resolution :",         .u = { .ch = { .value = (int8_t *) &resolution,  .choice_count = 2, .choices = FormChoiceField::resolution_choices     } }, .entry_type = FormEntryType::HORIZONTAL  },
  { .caption = "Show Battery Level :",       .u = { .ch = { .value = &show_battery,           .choice_count = 4, .choices = FormChoiceField::battery_visual_choices } }, .entry_type = FormEntryType::VERTICAL    },
  { .caption = "Show Title (*):",            .u = { .ch = { .value = &show_title,             .choice_count = 2, .choices = FormChoiceField::yes_no_choices         } }, .entry_type = FormEntryType::HORIZONTAL  },
  #if DATE_TIME_RTC
    { .caption = "Right Bottom Corner :",    .u = { .ch = { .value = &show_heap_or_rtc,       .choice_count = 3, .choices = FormChoiceField::right_corner_choices   } }, .entry_type = FormEntryType::HORIZONTAL  },
  #else
    { .caption = "Show Heap Sizes :",        .u = { .ch = { .value = &show_heap,              .choice_count = 2, .choices = FormChoiceField::yes_no_choices         } }, .entry_type = FormEntryType::HORIZONTAL  },
  #endif
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    { .caption = nullptr,                    .u = { .ch = { .value = &done,                   .choice_count = 0, .choices = nullptr                                 } }, .entry_type = FormEntryType::DONE        }
  #endif
 };

#if INKPLATE_6PLUS || TOUCH_TRIAL
  static constexpr int8_t FONT_FORM_SIZE = 5;
#else
  static constexpr int8_t FONT_FORM_SIZE = 4;
#endif
static FormEntry font_params_form_entries[FONT_FORM_SIZE] = {
  { .caption = "Default Font Size (*):",      .u = { .ch = { .value = &font_size,          .choice_count = 4, .choices = FormChoiceField::font_size_choices } }, .entry_type = FormEntryType::HORIZONTAL },
  { .caption = "Use Fonts in E-books (*):",   .u = { .ch = { .value = &use_fonts_in_books, .choice_count = 2, .choices = FormChoiceField::yes_no_choices    } }, .entry_type = FormEntryType::HORIZONTAL },
  { .caption = "Default Font (*):",           .u = { .ch = { .value = &default_font,       .choice_count = 8, .choices = FormChoiceField::font_choices      } }, .entry_type = FormEntryType::VERTICAL   },
  { .caption = "Show Images in E-books (*):", .u = { .ch = { .value = &show_images,        .choice_count = 2, .choices = FormChoiceField::yes_no_choices    } }, .entry_type = FormEntryType::HORIZONTAL },
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    { .caption = nullptr,                     .u = { .ch = { .value = &done,               .choice_count = 0, .choices = nullptr                            } }, .entry_type = FormEntryType::DONE       }
  #endif
};

#if DATE_TIME_RTC
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    static constexpr int8_t DATE_TIME_FORM_SIZE = 7;
  #else
    static constexpr int8_t DATE_TIME_FORM_SIZE = 6;
  #endif

  static FormEntry date_time_form_entries[DATE_TIME_FORM_SIZE] = {
    { .caption = "Year :",   .u = { .val = { .value = &year,   .min = 2022, .max = 2099              } }, .entry_type = FormEntryType::UINT16  },
    { .caption = "Month :",  .u = { .val = { .value = &month,  .min =    1, .max =   12              } }, .entry_type = FormEntryType::UINT16  },
    { .caption = "Day :",    .u = { .val = { .value = &day,    .min =    1, .max =   31              } }, .entry_type = FormEntryType::UINT16  },
    { .caption = "Hour :",   .u = { .val = { .value = &hour,   .min =    0, .max =   23              } }, .entry_type = FormEntryType::UINT16  },
    { .caption = "Minute :", .u = { .val = { .value = &minute, .min =    0, .max =   59              } }, .entry_type = FormEntryType::UINT16  },
    { .caption = "Second :", .u = { .val = { .value = &second, .min =    0, .max =   59              } }, .entry_type = FormEntryType::UINT16  },

    #if INKPLATE_6PLUS || TOUCH_TRIAL
      { .caption = nullptr,  .u = { .ch  = { .value = &done,   .choice_count = 0, .choices = nullptr } }, .entry_type = FormEntryType::DONE    }
    #endif
  };
#endif

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
  config.get(Config::Ident::TIMEOUT,          &timeout               );

  #if DATE_TIME_RTC
    int8_t show_heap, show_rtc;
    config.get(Config::Ident::SHOW_RTC,       &show_rtc              );
    config.get(Config::Ident::SHOW_HEAP,      &show_heap             );

    show_heap_or_rtc = (show_rtc != 0) ? 1 : ((show_heap != 0) ? 2 : 0);
  #else
    config.get(Config::Ident::SHOW_HEAP,      &show_heap             );
  #endif

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

static void
init_nvs()
{
  menu_viewer.clear_highlight();
  #if EPUB_INKPLATE_BUILD
    if (nvs_mgr.setup(true)) {
      msg_viewer.show(
        MsgViewer::BOOK, 
        false,
        false,
        "E-Books History Cleared", 
        "The E-Books History has been initialized with success.");
    }
    else {
      msg_viewer.show(
        MsgViewer::BOOK, 
        false,
        false,
        "E-Books History Clearing Error", 
        "The E-Books History has not been initialized properly. "
        "Potential hardware problem or software framework issue.");
    }
  #endif
}

#if INKPLATE_6PLUS
  static void goto_next();
  static void goto_prev();

  static void 
  calibrate()
  {
    event_mgr.show_calibration();
    option_controller.set_calibration_is_shown();
  }
#endif

#if DATE_TIME_RTC
  static void 
  clock_adjust_form()
  {
    #if INKPLATE_6PLUS
      constexpr uint8_t DD[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
      RTC::WeekDay tmp;
      uint8_t m, d, h, mm, s;
      rtc.get_date_time(year, m, d, h, mm, s, tmp);
      if ((year < 2022) || (year > 2099)) year = 2022;
      month  = ((m < 1) || (m > 12)) ? 1 : m;

      uint8_t max = ((m != 2) || ((year & 3) != 0)) ? DD[m - 1] : 29;

      day    = ((d < 1) || (d > max)) ? 1 : d;
      hour   =  (h > 23) ? 12 :  h;
      minute =  (mm > 59) ? 0 : mm;
      second =  (s  > 59) ? 0 :  s;
    #endif

    form_viewer.show(date_time_form_entries, DATE_TIME_FORM_SIZE, "Hour is in 24 hours format.");
    option_controller.set_date_time_form_is_shown();
  }

  static void
  set_clock()
  {
    #if INKPLATE_6PLUS
      auto dow = [=](uint16_t y, uint16_t m, uint16_t d) { 
        RTC::WeekDay wd = (RTC::WeekDay)((d += m < 3 ? y-- : y - 2, 23*m/9 + d + 4 + y/4- y/100 + y/400) % 7); 
        return wd;
      };
      rtc.set_date_time(year, month, day, hour, minute, second, dow(year, month, day));
    #endif
  }
#endif

#if EPUB_LINUX_BUILD && DEBUGGING
  void
  debugging()
  {
    #if DATE_TIME_RTC
      clock_adjust_form();
    #endif
  }
#endif

// IMPORTANT!!!
// The first (menu[0]) and the last menu entry (the one before END_MENU) MUST ALWAYS BE VISIBLE!!!

static MenuViewer::MenuEntry menu[] = {

  { MenuViewer::Icon::RETURN,      "Return to the e-books list",           CommonActions::return_to_last    , true,  true  },
  { MenuViewer::Icon::BOOK,        "Return to the last e-book being read", CommonActions::show_last_book    , true,  true  },
  { MenuViewer::Icon::MAIN_PARAMS, "Main parameters",                      main_parameters                  , true,  true  },
  { MenuViewer::Icon::FONT_PARAMS, "Default e-books parameters",           default_parameters               , true,  true  },
  { MenuViewer::Icon::WIFI,        "WiFi Access to the e-books folder",    wifi_mode                        , true,  true  },
  { MenuViewer::Icon::REFRESH,     "Refresh the e-books list",             CommonActions::refresh_books_dir , true,  true  },
  #if !INKPLATE_6PLUS
    { MenuViewer::Icon::REVERT,    "Clear e-books' read history",         init_nvs                          , true,  true  },
  #endif
  #if EPUB_LINUX_BUILD && DEBUGGING
    { MenuViewer::Icon::DEBUG,     "Debugging",                            debugging                        , true,  true  },
  #endif
  { MenuViewer::Icon::INFO,        "About the EPub-InkPlate application",  CommonActions::about             , true,  true  },
  { MenuViewer::Icon::POWEROFF,    "Power OFF (Deep Sleep)",               CommonActions::power_it_off      , true,  true  },
  #if INKPLATE_6PLUS
    { MenuViewer::Icon::NEXT_MENU, "Other options",                        goto_next                        , true,  true  },
  #endif
  { MenuViewer::Icon::END_MENU,     nullptr,                               nullptr                          , false, false }
};

#if INKPLATE_6PLUS
static MenuViewer::MenuEntry sub_menu[] = {
  { MenuViewer::Icon::PREV_MENU,    "Previous options",                    goto_prev                        , true,  true  },
  { MenuViewer::Icon::RETURN,       "Return to the e-books list",          CommonActions::return_to_last    , true,  true  },
  #if DATE_TIME_RTC
    { MenuViewer::Icon::CLOCK,      "Set Date/Time",                       clock_adjust_form                , true,  true  },
  #endif
  { MenuViewer::Icon::CALIB,        "Touch Screen Calibration",            calibrate                        , true,  false },
  { MenuViewer::Icon::REVERT,       "Clear e-books' read history",         init_nvs                         , true,  true  },
  { MenuViewer::Icon::END_MENU,     nullptr,                               nullptr                          , false, false }
};
#endif

void
OptionController::set_font_count(uint8_t count)
{
  font_params_form_entries[2].u.ch.choice_count = count;
}

void 
OptionController::enter()
{
  menu_viewer.show(menu);
  main_form_is_shown = false;
  font_form_is_shown = false;
}

#if INKPLATE_6PLUS
  static void 
  goto_next()
  {
    menu_viewer.show(sub_menu);
  }

  static void 
  goto_prev()
  {
    menu_viewer.show(menu);
  }
#endif

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
        config.put(Config::Ident::TIMEOUT,          timeout             );

        #if DATE_TIME_RTC
          config.put(Config::Ident::SHOW_HEAP,      (int8_t)(show_heap_or_rtc == 2 ? 1 : 0));
          config.put(Config::Ident::SHOW_RTC,       (int8_t)(show_heap_or_rtc == 1 ? 1 : 0));
        #else
          config.put(Config::Ident::SHOW_HEAP,      show_heap           );
        #endif

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

  #if DATE_TIME_RTC
    else if (date_time_form_is_shown) {
      if (form_viewer.event(event)) {
        date_time_form_is_shown = false;
        menu_viewer.clear_highlight();
        set_clock();
      }
    }
  #endif

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

  #if INKPLATE_6PLUS
  else if (calibration_is_shown) {
    if (event_mgr.calibration_event(event)) {
      calibration_is_shown = false;
      menu_viewer.show(menu, 0, true);
    }
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
