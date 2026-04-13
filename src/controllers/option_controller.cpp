// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __OPTION_CONTROLLER__ 1
#include "controllers/option_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "controllers/clock.hpp"
#include "controllers/common_actions.hpp"
#include "controllers/ntp.hpp"
#include "controllers/web_server.hpp"
#include "models/books_dir.hpp"
#include "models/config.hpp"
#include "models/epub.hpp"
#include "models/nvs_mgr.hpp"
#include "viewers/msg_viewer.hpp"

// #undef DEBUGGING
// #define DEBUGGING 1

#if EPUB_INKPLATE_BUILD
  #include "esp_system.h"
  #include "option_controller.hpp"
#endif

// static int8_t boolean_value;

static Screen::Orientation orientation;
static Screen::PixelResolution resolution;

static int8_t show_battery;
static int8_t timeout;
static int8_t show_pictures;
static int8_t font_size;
static int8_t use_fonts_in_books;
static int8_t default_font;
static int8_t show_title;
static int8_t dir_view;
static int8_t cover_size;
static int8_t done;

static Screen::Orientation old_orientation;
static Screen::PixelResolution old_resolution;
static int8_t old_show_pictures;
static int8_t old_font_size;
static int8_t old_use_fonts_in_books;
static int8_t old_default_font;
static int8_t old_show_title;
static int8_t old_dir_view;
static int8_t old_cover_size;

#if DATE_TIME_RTC
  static int8_t show_heap_or_rtc;
  static uint16_t year;
  static uint16_t month, day, hour, minute, second;
#else
  static int8_t show_heap;
#endif

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  static constexpr int8_t MAIN_FORM_SIZE = 9;
#else
  static constexpr int8_t MAIN_FORM_SIZE = 8;
#endif

static FormEntry main_params_form_entries[MAIN_FORM_SIZE] = {
    {.caption    = "Minutes Before Sleeping :",
     .u          = {.ch = {.value        = &timeout,
                           .choice_count = 3,
                           .choices      = FormChoiceField::timeout_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
    {.caption    = "Books Directory View :",
     .u          = {.ch = {.value        = &dir_view,
                           .choice_count = 2,
                           .choices      = FormChoiceField::dir_view_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
    {.caption    = "Books Cover Size :",
     .u          = {.ch = {.value        = &cover_size,
                           .choice_count = 3,
                           .choices      = FormChoiceField::cover_size_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    {.caption    = "uSDCard Position (*):",
     .u          = {.ch = {.value        = (int8_t *)&orientation,
                           .choice_count = 4,
                           .choices      = FormChoiceField::orientation_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
#else
    {.caption    = "Buttons Position (*):",
     .u          = {.ch = {.value        = (int8_t *)&orientation,
                           .choice_count = 3,
                           .choices      = FormChoiceField::orientation_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
#endif
    {.caption    = "Pixel Resolution :",
     .u          = {.ch = {.value        = (int8_t *)&resolution,
                           .choice_count = 2,
                           .choices      = FormChoiceField::resolution_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
    {.caption    = "Show Battery Level :",
     .u          = {.ch = {.value        = &show_battery,
                           .choice_count = 4,
                           .choices      = FormChoiceField::battery_visual_choices}},
     .entry_type = FormEntryType::VERTICAL},
    {.caption    = "Show Title (*):",
     .u          = {.ch = {.value        = &show_title,
                           .choice_count = 2,
                           .choices      = FormChoiceField::yes_no_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
#if DATE_TIME_RTC
    {.caption    = "Right Bottom Corner :",
     .u          = {.ch = {.value        = &show_heap_or_rtc,
                           .choice_count = 3,
                           .choices      = FormChoiceField::right_corner_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
#else
    {.caption    = "Show Heap Sizes :",
     .u          = {.ch = {.value        = &show_heap,
                           .choice_count = 2,
                           .choices      = FormChoiceField::yes_no_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
#endif
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    {.caption    = " DONE ",
     .u          = {.ch = {.value = &done, .choice_count = 0, .choices = nullptr}},
     .entry_type = FormEntryType::DONE}
#endif
};

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  static constexpr int8_t FONT_FORM_SIZE = 5;
#else
  static constexpr int8_t FONT_FORM_SIZE = 4;
#endif
static FormEntry font_params_form_entries[FONT_FORM_SIZE] = {
    {.caption    = "Default Font Size (*):",
     .u          = {.ch = {.value        = &font_size,
                           .choice_count = 4,
                           .choices      = FormChoiceField::font_size_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
    {.caption    = "Use Fonts in E-books (*):",
     .u          = {.ch = {.value        = &use_fonts_in_books,
                           .choice_count = 2,
                           .choices      = FormChoiceField::yes_no_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
    {.caption    = "Default Font (*):",
     .u          = {.ch = {.value        = &default_font,
                           .choice_count = 8,
                           .choices      = FormChoiceField::font_choices}},
     .entry_type = FormEntryType::VERTICAL},
    {.caption    = "Show Images in E-books (*):",
     .u          = {.ch = {.value        = &show_pictures,
                           .choice_count = 2,
                           .choices      = FormChoiceField::yes_no_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    {.caption    = " DONE ",
     .u          = {.ch = {.value = &done, .choice_count = 0, .choices = nullptr}},
     .entry_type = FormEntryType::DONE}
#endif
};

#if DATE_TIME_RTC
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    static constexpr int8_t DATE_TIME_FORM_SIZE = 7;
  #else
    static constexpr int8_t DATE_TIME_FORM_SIZE = 6;
  #endif

  static FormEntry date_time_form_entries[DATE_TIME_FORM_SIZE] = {
      {.caption    = "Year :",
       .u          = {.val = {.value = &year, .min = 2022, .max = 2099}},
       .entry_type = FormEntryType::UINT16},
      {.caption    = "Month :",
       .u          = {.val = {.value = &month, .min = 1, .max = 12}},
       .entry_type = FormEntryType::UINT16},
      {.caption    = "Day :",
       .u          = {.val = {.value = &day, .min = 1, .max = 31}},
       .entry_type = FormEntryType::UINT16},
      {.caption    = "Hour :",
       .u          = {.val = {.value = &hour, .min = 0, .max = 23}},
       .entry_type = FormEntryType::UINT16},
      {.caption    = "Minute :",
       .u          = {.val = {.value = &minute, .min = 0, .max = 59}},
       .entry_type = FormEntryType::UINT16},
      {.caption    = "Second :",
       .u          = {.val = {.value = &second, .min = 0, .max = 59}},
       .entry_type = FormEntryType::UINT16},

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      {.caption    = "DONE",
       .u          = {.ch = {.value = &done, .choice_count = 0, .choices = nullptr}},
       .entry_type = FormEntryType::DONE}
  #endif
  };
#endif

static constexpr char const *MAIN_PARAMS_CAPTION   = "Main parameters";
static constexpr char const *BOOK_DEFAULTS_CAPTION = "Default e-books parameters";
static constexpr char const *CLOCK_ADJUST_CAPTION  = "Set Date / Time";

void OptionController::main_parameters() {
  config.get(Config::Ident::ORIENTATION, (int8_t *)&orientation);
  config.get(Config::Ident::DIR_VIEW, &dir_view);
  config.get(Config::Ident::COVER_SIZE, &cover_size);
  config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *)&resolution);
  config.get(Config::Ident::BATTERY, &show_battery);
  config.get(Config::Ident::SHOW_TITLE, &show_title);
  config.get(Config::Ident::TIMEOUT, &timeout);

  #if DATE_TIME_RTC
    int8_t show_heap{0}, show_rtc{0};
    config.get(Config::Ident::SHOW_RTC, &show_rtc);
    config.get(Config::Ident::SHOW_HEAP, &show_heap);

    show_heap_or_rtc = (show_rtc != 0) ? 1 : ((show_heap != 0) ? 2 : 0);
  #else
    config.get(Config::Ident::SHOW_HEAP, &show_heap);
  #endif

  old_orientation = orientation;
  old_dir_view    = dir_view;
  old_cover_size  = cover_size;
  old_resolution  = resolution;
  old_show_title  = show_title;
  done            = 1;

  form_viewer->show(MAIN_PARAMS_CAPTION, main_params_form_entries, MAIN_FORM_SIZE,
                    "(*) Will trigger e-book pages location recalc.");

  option_controller.set_main_form_is_shown();
}

void OptionController::default_parameters() {
  config.get(Config::Ident::SHOW_PICTURES, &show_pictures);
  config.get(Config::Ident::FONT_SIZE, &font_size);
  config.get(Config::Ident::USE_FONTS_IN_BOOKS, &use_fonts_in_books);
  config.get(Config::Ident::DEFAULT_FONT, &default_font);

  old_show_pictures      = show_pictures;
  old_use_fonts_in_books = use_fonts_in_books;
  old_default_font       = default_font;
  old_font_size          = font_size;
  done                   = 1;

  form_viewer->show(BOOK_DEFAULTS_CAPTION, font_params_form_entries, FONT_FORM_SIZE,
                    "(*) Used as e-book default values.");

  option_controller.set_font_form_is_shown();
}

void OptionController::wifi_mode() {

  #if EPUB_INKPLATE_BUILD
    epub.close_file();
    fonts.clear(true);
    fonts.clear_glyph_caches();

    event_mgr.set_stay_on(true); // DO NOT sleep

    if (start_web_server(WebServerMode::STA)) {
      option_controller.set_wait_for_key_after_wifi(true);
    }
  #endif
}

void OptionController::init_nvs() {
  // menu_viewer.clear_highlight();
  #if EPUB_INKPLATE_BUILD
    if (nvs_mgr.setup(true)) {
      MsgViewer::show(MsgViewer::MsgType::BOOK, false, false, "E-Books History Cleared",
                      "The E-Books History has been initialized with success.");
    } else {
      MsgViewer::show(MsgViewer::MsgType::BOOK, false, false, "E-Books History Clearing Error",
                      "The E-Books History has not been initialized properly. "
                      "Potential hardware problem or software framework issue.");
    }
  #endif
}

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
  void OptionController::calibrate() {
    event_mgr.show_calibration();
    option_controller.set_calibration_is_shown();
  }
#endif

#if DATE_TIME_RTC
  void OptionController::clock_adjust_form() {
    time_t t;
    tm tim;
    Clock::get_date_time(t);

    localtime_r(&t, &tim);

    year   = tim.tm_year + 1900;
    month  = tim.tm_mon + 1;
    day    = tim.tm_mday;
    hour   = tim.tm_hour;
    minute = tim.tm_min;
    second = tim.tm_sec;

    form_viewer->show(CLOCK_ADJUST_CAPTION, date_time_form_entries, DATE_TIME_FORM_SIZE,
                      "Hour is in 24 hours format.");
    option_controller.set_date_time_form_is_shown();
  }

  void OptionController::set_clock() {
    time_t t;
    tm tim;

    tim = {.tm_sec   = second,
           .tm_min   = minute,
           .tm_hour  = hour,
           .tm_mday  = day,
           .tm_mon   = month - 1,
           .tm_year  = year - 1900,
           .tm_wday  = 0,
           .tm_yday  = 0,
           .tm_isdst = -1};

    t = mktime(&tim);
    Clock::set_date_time(t);
  }

  void OptionController::ntp_clock_adjust() {

    // page_locs.abort_threads();
    // epub.close_file();

    std::string ntp_server;
    config.get(Config::Ident::NTP_SERVER, ntp_server);

    MsgViewer::show(MsgViewer::MsgType::NTP_CLOCK, false, true, "Date/Time Retrival",
                    "Retrieving Date and Time from NTP Server %s. Please wait.",
                    ntp_server.c_str());

    if (ntp.get_and_set_time()) {
      time_t time;
      Clock::get_date_time(time);
      MsgViewer::show(MsgViewer::MsgType::NTP_CLOCK, true, true, "Date/Time Retrival Completed",
                      "Local Time is %s. The device will now restart.", ctime(&time));
    } else {
      MsgViewer::show(MsgViewer::MsgType::NTP_CLOCK, true, true, "Date/Time Retrival Failed",
                      "Unable to get Date/Time from NTP Server! The device will now restart.");
    }

    option_controller.set_wait_for_key_after_wifi(false);
  }
#endif

#if DEBUGGING
  void OptionController::debugging() {
    auto [page, progress_data] = MsgViewer::show_progress(
        "A small test to check the show progress capability. Please wait...");
    for (int i = 0; i <= 10; i++) {
      std::tie(page, progress_data) =
          MsgViewer::update_progress(std::move(page), std::move(progress_data), i * 10);
      sleep(1);
    }
  }
#endif

// IMPORTANT!!!
// The first (menu[0]) and the last menu entry (the one before END_MENU) MUST ALWAYS BE VISIBLE!!!

// clang-format off
static MenuViewer::MenuEntry menu[] = {

  {MenuViewer::Icon::RETURN,      true,  true,  nullptr, "Return to the e-books list"},
  {MenuViewer::Icon::BOOK,        true,  true,  nullptr, "Return to the last e-book being read"},
  {MenuViewer::Icon::MAIN_PARAMS, true,  true,  nullptr,  MAIN_PARAMS_CAPTION},
  {MenuViewer::Icon::FONT_PARAMS, true,  true,  nullptr,  BOOK_DEFAULTS_CAPTION},
  {MenuViewer::Icon::WIFI,        true,  true,  nullptr, "WiFi Access to the e-books folder"},
  {MenuViewer::Icon::REFRESH,     true,  true,  nullptr, "Refresh the e-books list"},
  {MenuViewer::Icon::CLR_HISTORY, true,  true,  nullptr, "Clear e-books' read history"},
#if DATE_TIME_RTC
  {MenuViewer::Icon::CLOCK,       true,  true,  nullptr,  CLOCK_ADJUST_CAPTION},
  {MenuViewer::Icon::NTP_CLOCK,   true,  true,  nullptr,  "Retrieve Date/Time from Time Server"},
#endif

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
  {MenuViewer::Icon::CALIB,       true,  false, nullptr,  "Touch Screen Calibration"},
  {MenuViewer::Icon::CLR_HISTORY, true,  true,  nullptr,  "Clear e-books' read history"},
#elif MENU_6PLUS
  {MenuViewer::Icon::CALIB,       true,  false, nullptr,  "Touch Screen Calibration"},
  {MenuViewer::Icon::CLR_HISTORY, true,  true,  nullptr,  "Clear e-books' read history"},
#endif
#if DEBUGGING
  {MenuViewer::Icon::DEBUG,       true,  true,  nullptr,  "Debugging"},
#endif
  {MenuViewer::Icon::INFO,        true,  true,  nullptr,  "About the EPub-InkPlate application"},
  // This entry must be the last one before END_MENU and MUST ALWAYS BE VISIBLE!!
  {MenuViewer::Icon::POWEROFF,    true,  true,  nullptr,  "Power OFF (Deep Sleep)"},
  {MenuViewer::Icon::END_MENU,    false, false, nullptr, nullptr}};

// clang-format on

void OptionController::set_font_count(uint8_t count) {
  font_params_form_entries[2].u.ch.choice_count = count;
}

void OptionController::enter() {
  menu_viewer = MenuViewer::Make();
  form_viewer = FormViewer::Make();

  int idx = 0;
  while (menu[idx].icon != MenuViewer::Icon::END_MENU) {
    switch (menu[idx].icon) {
    case MenuViewer::Icon::RETURN:
      menu[idx].func = CommonActions::return_to_last;
      break;
    case MenuViewer::Icon::BOOK:
      menu[idx].func = CommonActions::show_last_book;
      break;
    case MenuViewer::Icon::MAIN_PARAMS:
      menu[idx].func = [this]() { this->main_parameters(); };
      break;
    case MenuViewer::Icon::FONT_PARAMS:
      menu[idx].func = [this]() { this->default_parameters(); };
      break;
    case MenuViewer::Icon::WIFI:
      menu[idx].func = [this]() { this->wifi_mode(); };
      break;
    case MenuViewer::Icon::REFRESH:
      menu[idx].func = CommonActions::refresh_books_dir;
      break;
    case MenuViewer::Icon::CLR_HISTORY:
      menu[idx].func = [this]() { this->init_nvs(); };
      break;
      #if DATE_TIME_RTC
      case MenuViewer::Icon::CLOCK:
        menu[idx].func = [this]() { this->clock_adjust_form(); };
        break;
      case MenuViewer::Icon::NTP_CLOCK:
        menu[idx].func = [this]() { this->ntp_clock_adjust(); };
        break;
      #endif
      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
      case MenuViewer::Icon::CALIB:
        menu[idx].func = [this]() { this->calibrate(); };
        break;
      #elif MENU_6PLUS
      case MenuViewer::Icon::CALIB:
        menu[idx].func = [this]() { this->calibrate(); };
        break;
      #endif
      #if DEBUGGING
      case MenuViewer::Icon::DEBUG:
        menu[idx].func = [this]() { this->debugging(); };
        break;
      #endif
    case MenuViewer::Icon::INFO:
      menu[idx].func = CommonActions::about;
      break;
    case MenuViewer::Icon::POWEROFF:
      menu[idx].func = CommonActions::power_it_off;
      break;
    default:
      break;
    }

    idx++;
  }

  if (menu_viewer) {
    menu_viewer->show(menu);
  }
  main_form_is_shown = false;
  font_form_is_shown = false;
}

void OptionController::leave(bool going_to_deep_sleep) {
  menu_viewer.reset();
  form_viewer.reset();
}

void OptionController::input_event(const EventMgr::Event &event) {
  if (main_form_is_shown) {
    if (form_viewer->event(event)) {
      main_form_is_shown = false;
      // if (ok) {
      config.put(Config::Ident::ORIENTATION, static_cast<int8_t>(orientation));
      config.put(Config::Ident::DIR_VIEW, dir_view);
      config.put(Config::Ident::COVER_SIZE, cover_size);
      config.put(Config::Ident::PIXEL_RESOLUTION, static_cast<int8_t>(resolution));
      config.put(Config::Ident::BATTERY, show_battery);
      config.put(Config::Ident::SHOW_TITLE, show_title);
      config.put(Config::Ident::TIMEOUT, timeout);

      #if DATE_TIME_RTC
        config.put(Config::Ident::SHOW_HEAP, static_cast<int8_t>(show_heap_or_rtc == 2 ? 1 : 0));
        config.put(Config::Ident::SHOW_RTC, static_cast<int8_t>(show_heap_or_rtc == 1 ? 1 : 0));
      #else
        config.put(Config::Ident::SHOW_HEAP, show_heap);
      #endif

      config.save();

      if (old_orientation != orientation) {
        screen.set_orientation(orientation);
        event_mgr.set_orientation(orientation);
        books_dir_controller.new_orientation();
      }

      if (old_dir_view != dir_view) {
        books_dir_controller.set_current_book_index(-1);
      }

      if (old_cover_size != cover_size) {
        int16_t dummy;
        books_dir.refresh(nullptr, dummy, true);
        menu_viewer->show(menu, 2, true);
      }

      if (old_resolution != resolution) {
        fonts.clear_glyph_caches();
        screen.set_pixel_resolution(resolution);
      }

      // if ((old_orientation != orientation) || (old_show_title != show_title)) {
      //   epub.update_book_format_params();
      // }

      if ((old_orientation != orientation) || (old_resolution != resolution)) {
        menu_viewer->show(menu, 2, true);
      } else {
        menu_viewer->clear_highlight();
      }
      // }
      menu_viewer->show(menu, 2, true);
    }
  } else if (font_form_is_shown) {
    if (form_viewer->event(event)) {
      font_form_is_shown = false;
      // if (ok) {
      config.put(Config::Ident::SHOW_PICTURES, show_pictures);
      config.put(Config::Ident::FONT_SIZE, font_size);
      config.put(Config::Ident::DEFAULT_FONT, default_font);
      config.put(Config::Ident::USE_FONTS_IN_BOOKS, use_fonts_in_books);
      config.save();

      // if ((old_show_pictures != show_pictures) || (old_font_size != font_size) ||
      //     (old_default_font != default_font) || (old_use_fonts_in_books != use_fonts_in_books)) {
      //   epub.update_book_format_params();
      // }

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
      // menu_viewer.clear_highlight();
      menu_viewer->show(menu, 3, true);
    }
  }

  #if DATE_TIME_RTC
  else if (date_time_form_is_shown) {
      if (form_viewer->event(event)) {
        date_time_form_is_shown = false;
        menu_viewer->clear_highlight();
        set_clock();
        menu_viewer->show(menu, 7, true);
      }
    }
  #endif

  #if EPUB_INKPLATE_BUILD
    else if (wait_for_key_after_wifi) {
      MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Restarting",
                      "The device is now restarting. Please wait.");
      wait_for_key_after_wifi = false;
      if (web_server_was_started) {
        stop_web_server();
      }
      if (books_refresh_needed) {
        books_refresh_needed = false;
        int16_t dummy;
        books_dir.refresh(nullptr, dummy, true);
      }
      inkplate_platform.restart();
    }
  #endif

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    else if (calibration_is_shown) {
      if (event_mgr.calibration_event(event)) {
        calibration_is_shown = false;
        menu_viewer->show(menu, 0, true);
      }
    }
  #endif

    else {
    if (menu_viewer->event(event)) {
      if (books_refresh_needed) {
        books_refresh_needed = false;
        int16_t dummy;
        books_dir.refresh(nullptr, dummy, true);
      }
      app_controller.set_controller(AppController::Ctrl::LAST);
    }
  }
}
