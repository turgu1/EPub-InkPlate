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

static int8_t showBattery;
static int8_t timeout;
static int8_t showPictures;
static int8_t fontSize;
static int8_t useFontsInBooks;
static int8_t defaultFont;
static int8_t showTitle;
static int8_t dirView;
static int8_t coverSize;
static int8_t done;

static Screen::Orientation oldOrientation;
static Screen::PixelResolution oldResolution;
static int8_t oldShowPictures;
static int8_t oldFontSize;
static int8_t oldUseFontsInBooks;
static int8_t oldDefaultFont;
static int8_t oldShowTitle;
static int8_t oldDirView;
static int8_t oldCoverSize;

#if DATE_TIME_RTC
  static int8_t showHeapOrRtc;
  static uint16_t year;
  static uint16_t month, day, hour, minute, second;
#else
  static int8_t showHeap;
#endif

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  static constexpr int8_t MAIN_FORM_SIZE = 9;
#else
  static constexpr int8_t MAIN_FORM_SIZE = 8;
#endif

static FormEntry mainParamsFormEntries[MAIN_FORM_SIZE] = {
    {.caption    = "Minutes Before Sleeping :",
     .u          = {.ch = {.value        = &timeout,
                           .choiceCount = 3,
                           .choices      = FormChoiceField::timeoutChoices}},
     .entryType = FormEntryType::HORIZONTAL},
    {.caption    = "Books Directory View :",
     .u          = {.ch = {.value        = &dirView,
                           .choiceCount = 2,
                           .choices      = FormChoiceField::dirViewChoices}},
     .entryType = FormEntryType::HORIZONTAL},
    {.caption    = "Books Cover Size :",
     .u          = {.ch = {.value        = &coverSize,
                           .choiceCount = 3,
                           .choices      = FormChoiceField::coverSizeChoices}},
     .entryType = FormEntryType::HORIZONTAL},
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    {.caption    = "uSDCard Position (*):",
     .u          = {.ch = {.value        = (int8_t *)&orientation,
                           .choiceCount = 4,
                           .choices      = FormChoiceField::orientationChoices}},
     .entryType = FormEntryType::HORIZONTAL},
#else
    {.caption    = "Buttons Position (*):",
     .u          = {.ch = {.value        = (int8_t *)&orientation,
                           .choiceCount = 3,
                           .choices      = FormChoiceField::orientationChoices}},
     .entryType = FormEntryType::HORIZONTAL},
#endif
    {.caption    = "Pixel Resolution :",
     .u          = {.ch = {.value        = (int8_t *)&resolution,
                           .choiceCount = 2,
                           .choices      = FormChoiceField::resolutionChoices}},
     .entryType = FormEntryType::HORIZONTAL},
    {.caption    = "Show Battery Level :",
     .u          = {.ch = {.value        = &showBattery,
                           .choiceCount = 4,
                           .choices      = FormChoiceField::batteryVisualChoices}},
     .entryType = FormEntryType::VERTICAL},
    {.caption    = "Show Title (*):",
     .u          = {.ch = {.value        = &showTitle,
                           .choiceCount = 2,
                           .choices      = FormChoiceField::yesNoChoices}},
     .entryType = FormEntryType::HORIZONTAL},
#if DATE_TIME_RTC
    {.caption    = "Right Bottom Corner :",
     .u          = {.ch = {.value        = &showHeapOrRtc,
                           .choiceCount = 3,
                           .choices      = FormChoiceField::rightCornerChoices}},
     .entryType = FormEntryType::HORIZONTAL},
#else
    {.caption    = "Show Heap Sizes :",
     .u          = {.ch = {.value        = &showHeap,
                           .choiceCount = 2,
                           .choices      = FormChoiceField::yesNoChoices}},
     .entryType = FormEntryType::HORIZONTAL},
#endif
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    {.caption    = " DONE ",
     .u          = {.ch = {.value = &done, .choiceCount = 0, .choices = nullptr}},
     .entryType = FormEntryType::DONE}
#endif
};

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  static constexpr int8_t FONT_FORM_SIZE = 5;
#else
  static constexpr int8_t FONT_FORM_SIZE = 4;
#endif
static FormEntry fontParamsFormEntries[FONT_FORM_SIZE] = {
    {.caption    = "Default Font Size (*):",
     .u          = {.ch = {.value        = &fontSize,
                           .choiceCount = 4,
                           .choices      = FormChoiceField::fontSizeChoices}},
     .entryType = FormEntryType::HORIZONTAL},
    {.caption    = "Use Fonts in E-books (*):",
     .u          = {.ch = {.value        = &useFontsInBooks,
                           .choiceCount = 2,
                           .choices      = FormChoiceField::yesNoChoices}},
     .entryType = FormEntryType::HORIZONTAL},
    {.caption    = "Default Font (*):",
     .u          = {.ch = {.value        = &defaultFont,
                           .choiceCount = 8,
                           .choices      = FormChoiceField::fontChoices}},
     .entryType = FormEntryType::VERTICAL},
    {.caption    = "Show Images in E-books (*):",
     .u          = {.ch = {.value        = &showPictures,
                           .choiceCount = 2,
                           .choices      = FormChoiceField::yesNoChoices}},
     .entryType = FormEntryType::HORIZONTAL},
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    {.caption    = " DONE ",
     .u          = {.ch = {.value = &done, .choiceCount = 0, .choices = nullptr}},
     .entryType = FormEntryType::DONE}
#endif
};

#if DATE_TIME_RTC
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    static constexpr int8_t DATE_TIME_FORM_SIZE = 7;
  #else
    static constexpr int8_t DATE_TIME_FORM_SIZE = 6;
  #endif

  static FormEntry dateTimeFormEntries[DATE_TIME_FORM_SIZE] = {
      {.caption    = "Year :",
       .u          = {.val = {.value = &year, .min = 2022, .max = 2099}},
       .entryType = FormEntryType::UINT16},
      {.caption    = "Month :",
       .u          = {.val = {.value = &month, .min = 1, .max = 12}},
       .entryType = FormEntryType::UINT16},
      {.caption    = "Day :",
       .u          = {.val = {.value = &day, .min = 1, .max = 31}},
       .entryType = FormEntryType::UINT16},
      {.caption    = "Hour :",
       .u          = {.val = {.value = &hour, .min = 0, .max = 23}},
       .entryType = FormEntryType::UINT16},
      {.caption    = "Minute :",
       .u          = {.val = {.value = &minute, .min = 0, .max = 59}},
       .entryType = FormEntryType::UINT16},
      {.caption    = "Second :",
       .u          = {.val = {.value = &second, .min = 0, .max = 59}},
       .entryType = FormEntryType::UINT16},

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      {.caption    = "DONE",
       .u          = {.ch = {.value = &done, .choiceCount = 0, .choices = nullptr}},
       .entryType = FormEntryType::DONE}
  #endif
  };
#endif

static constexpr char const *MAIN_PARAMS_CAPTION   = "Main parameters";
static constexpr char const *BOOK_DEFAULTS_CAPTION = "Default e-books parameters";
static constexpr char const *CLOCK_ADJUST_CAPTION  = "Set Date / Time";

auto OptionController::mainParameters() -> void {
  config.get(Config::Ident::ORIENTATION, (int8_t *)&orientation);
  config.get(Config::Ident::DIR_VIEW, &dirView);
  config.get(Config::Ident::COVER_SIZE, &coverSize);
  config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *)&resolution);
  config.get(Config::Ident::BATTERY, &showBattery);
  config.get(Config::Ident::SHOW_TITLE, &showTitle);
  config.get(Config::Ident::TIMEOUT, &timeout);

  #if DATE_TIME_RTC
    int8_t showHeap{0}, showRtc{0};
    config.get(Config::Ident::SHOW_RTC, &showRtc);
    config.get(Config::Ident::SHOW_HEAP, &showHeap);

    showHeapOrRtc = (showRtc != 0) ? 1 : ((showHeap != 0) ? 2 : 0);
  #else
    config.get(Config::Ident::SHOW_HEAP, &showHeap);
  #endif

  oldOrientation = orientation;
  oldDirView     = dirView;
  oldCoverSize   = coverSize;
  oldResolution  = resolution;
  oldShowTitle   = showTitle;
  done           = 1;

  formViewer->show(MAIN_PARAMS_CAPTION, mainParamsFormEntries, MAIN_FORM_SIZE,
                   "(*) Will trigger e-book pages location recalc.");

  optionController.setMainFormIsShown();
}

auto OptionController::defaultParameters() -> void {
  config.get(Config::Ident::SHOW_PICTURES, &showPictures);
  config.get(Config::Ident::FONT_SIZE, &fontSize);
  config.get(Config::Ident::USE_FONTS_IN_BOOKS, &useFontsInBooks);
  config.get(Config::Ident::DEFAULT_FONT, &defaultFont);

  oldShowPictures    = showPictures;
  oldUseFontsInBooks = useFontsInBooks;
  oldDefaultFont     = defaultFont;
  oldFontSize        = fontSize;
  done               = 1;

  formViewer->show(BOOK_DEFAULTS_CAPTION, fontParamsFormEntries, FONT_FORM_SIZE,
                   "(*) Used as e-book default values.");

  optionController.setFontFormIsShown();
}

auto OptionController::wifiMode() -> void {

  #if EPUB_INKPLATE_BUILD
    fonts.clear(true);
    fonts.clearGlyphCaches();

    eventMgr.setStayOn(true); // DO NOT sleep

    if (startWebServer(WebServerMode::STA)) {
      optionController.setWaitForKeyAfterWifi(true);
    }
  #endif
}

auto OptionController::initNvs() -> void {
  // menuViewer.clearHighlight();
  #if EPUB_INKPLATE_BUILD
    if (nvsMgr.setup(true)) {
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
  auto OptionController::calibrate() -> void {
    eventMgr.showCalibration();
    optionController.setCalibrationIsShown();
  }
#endif

#if DATE_TIME_RTC
  auto OptionController::clockAdjustForm() -> void {
    time_t t;
    tm tim;
    Clock::getDateTime(t);

    localtime_r(&t, &tim);

    year   = tim.tm_year + 1900;
    month  = tim.tm_mon + 1;
    day    = tim.tm_mday;
    hour   = tim.tm_hour;
    minute = tim.tm_min;
    second = tim.tm_sec;

    formViewer->show(CLOCK_ADJUST_CAPTION, dateTimeFormEntries, DATE_TIME_FORM_SIZE,
                     "Hour is in 24 hours format.");
    optionController.setDateTimeFormIsShown();
  }

  auto OptionController::setClock() -> void {
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
    Clock::setDateTime(t);
  }

  auto OptionController::ntpClockAdjust() -> void {

    // pageLocs.abort_threads();
    // epub.closeFile();

    std::string ntpServer;
    config.get(Config::Ident::NTP_SERVER, ntpServer);

    MsgViewer::show(MsgViewer::MsgType::NTP_CLOCK, false, true, "Date/Time Retrival",
                    "Retrieving Date and Time from NTP Server %s. Please wait.", ntpServer.c_str());

    if (ntp.getAndSetTime()) {
      time_t time;
      Clock::getDateTime(time);
      MsgViewer::show(MsgViewer::MsgType::NTP_CLOCK, true, true, "Date/Time Retrival Completed",
                      "Local Time is %s. The device will now restart.", ctime(&time));
    } else {
      MsgViewer::show(MsgViewer::MsgType::NTP_CLOCK, true, true, "Date/Time Retrival Failed",
                      "Unable to get Date/Time from NTP Server! The device will now restart.");
    }

    optionController.setWaitForKeyAfterWifi(false);
  }
#endif

#if DEBUGGING
  auto OptionController::debugging() -> void {
    auto [page, progressData] = MsgViewer::showProgress(
        "A small test to check the show progress capability. Please wait...");
    for (int i = 0; i <= 10; i++) {
      std::tie(page, progressData) =
          MsgViewer::updateProgress(std::move(page), std::move(progressData), i * 10);
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

auto OptionController::setFontCount(uint8_t count) -> void {
  fontParamsFormEntries[2].u.ch.choiceCount = count;
}

auto OptionController::enter() -> void {
  menuViewer = MenuViewer::Make();
  formViewer = FormViewer::Make();

  int idx = 0;
  while (menu[idx].icon != MenuViewer::Icon::END_MENU) {
    switch (menu[idx].icon) {
    case MenuViewer::Icon::RETURN:
      menu[idx].func = CommonActions::returnToLast;
      break;
    case MenuViewer::Icon::BOOK:
      menu[idx].func = CommonActions::showLastBook;
      break;
    case MenuViewer::Icon::MAIN_PARAMS:
      menu[idx].func = [this]() { this->mainParameters(); };
      break;
    case MenuViewer::Icon::FONT_PARAMS:
      menu[idx].func = [this]() { this->defaultParameters(); };
      break;
    case MenuViewer::Icon::WIFI:
      menu[idx].func = [this]() { this->wifiMode(); };
      break;
    case MenuViewer::Icon::REFRESH:
      menu[idx].func = CommonActions::refreshBooksDir;
      break;
    case MenuViewer::Icon::CLR_HISTORY:
      menu[idx].func = [this]() { this->initNvs(); };
      break;
      #if DATE_TIME_RTC
      case MenuViewer::Icon::CLOCK:
        menu[idx].func = [this]() { this->clockAdjustForm(); };
        break;
      case MenuViewer::Icon::NTP_CLOCK:
        menu[idx].func = [this]() { this->ntpClockAdjust(); };
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
      menu[idx].func = CommonActions::powerItOff;
      break;
    default:
      break;
    }

    idx++;
  }

  if (menuViewer) {
    menuViewer->show(menu);
  }
  mainFormIsShown = false;
  fontFormIsShown = false;
}

auto OptionController::leave(bool goingToDeepSleep) -> void {
  menuViewer.reset();
  formViewer.reset();
}

auto OptionController::inputEvent(const EventMgr::Event &event) -> void {
  if (mainFormIsShown) {
    if (formViewer->event(event)) {
      mainFormIsShown = false;
      // if (ok) {
      config.put(Config::Ident::ORIENTATION, static_cast<int8_t>(orientation));
      config.put(Config::Ident::DIR_VIEW, dirView);
      config.put(Config::Ident::COVER_SIZE, coverSize);
      config.put(Config::Ident::PIXEL_RESOLUTION, static_cast<int8_t>(resolution));
      config.put(Config::Ident::BATTERY, showBattery);
      config.put(Config::Ident::SHOW_TITLE, showTitle);
      config.put(Config::Ident::TIMEOUT, timeout);

      #if DATE_TIME_RTC
        config.put(Config::Ident::SHOW_HEAP, static_cast<int8_t>(showHeapOrRtc == 2 ? 1 : 0));
        config.put(Config::Ident::SHOW_RTC, static_cast<int8_t>(showHeapOrRtc == 1 ? 1 : 0));
      #else
        config.put(Config::Ident::SHOW_HEAP, showHeap);
      #endif

      config.save();

      if (oldOrientation != orientation) {
        screen.setOrientation(orientation);
        eventMgr.setOrientation(orientation);
        booksDirController.newOrientation();
      }

      if (oldDirView != dirView) {
        booksDirController.setCurrentBookIndex(-1);
      }

      if (oldCoverSize != coverSize) {
        int16_t dummy;
        booksDir.refresh(nullptr, dummy, true);
        menuViewer->show(menu, 2, true);
      }

      if (oldResolution != resolution) {
        fonts.clearGlyphCaches();
        screen.setPixelResolution(resolution);
      }

      // if ((oldOrientation != orientation) || (oldShowTitle != showTitle)) {
      //   epub.update_book_format_params();
      // }

      if ((oldOrientation != orientation) || (oldResolution != resolution)) {
        menuViewer->show(menu, 2, true);
      } else {
        menuViewer->clearHighlight();
      }
      // }
      menuViewer->show(menu, 2, true);
    }
  } else if (fontFormIsShown) {
    if (formViewer->event(event)) {
      fontFormIsShown = false;
      // if (ok) {
      config.put(Config::Ident::SHOW_PICTURES, showPictures);
      config.put(Config::Ident::FONT_SIZE, fontSize);
      config.put(Config::Ident::DEFAULT_FONT, defaultFont);
      config.put(Config::Ident::USE_FONTS_IN_BOOKS, useFontsInBooks);
      config.save();

      // if ((oldShowPictures != showPictures) || (oldFontSize != fontSize) ||
      //     (oldDefaultFont != defaultFont) || (oldUseFontsInBooks != useFontsInBooks)) {
      //   epub.update_book_format_params();
      // }

      if (oldDefaultFont != defaultFont) {
        fonts.adjustDefaultFont(defaultFont);
      }

      if (oldUseFontsInBooks != useFontsInBooks) {
        if (useFontsInBooks == 0) {
          fonts.clear();
          fonts.clearGlyphCaches();
        }
      }
      // }
      // menuViewer.clearHighlight();
      menuViewer->show(menu, 3, true);
    }
  }

  #if DATE_TIME_RTC
  else if (dateTimeFormIsShown) {
      if (formViewer->event(event)) {
        dateTimeFormIsShown = false;
        menuViewer->clearHighlight();
        setClock();
        menuViewer->show(menu, 7, true);
      }
    }
  #endif

  #if EPUB_INKPLATE_BUILD
    else if (waitForKeyAfterWifi) {
      MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Restarting",
                      "The device is now restarting. Please wait.");
      waitForKeyAfterWifi = false;
      if (webServerWasStarted) {
        stopWebServer();
      }
      if (booksRefreshNeeded) {
        booksRefreshNeeded = false;
        int16_t dummy;
        booksDir.refresh(nullptr, dummy, true);
      }
      inkplate_platform.restart();
    }
  #endif

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    else if (calibrationIsShown) {
      if (eventMgr.calibrationEvent(event)) {
        calibrationIsShown = false;
        menuViewer->show(menu, 0, true);
      }
    }
  #endif

    else {
    if (menuViewer->event(event)) {
      if (booksRefreshNeeded) {
        booksRefreshNeeded = false;
        int16_t dummy;
        booksDir.refresh(nullptr, dummy, true);
      }
      appController.setController(AppController::Ctrl::LAST);
    }
  }
}
