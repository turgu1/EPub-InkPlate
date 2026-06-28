// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"
#include "viewers/form_viewer.hpp"
#include "viewers/menu_viewer.hpp"

class OptionController {
private:
  static constexpr char const *TAG = "OptionController";

  bool mainFormIsShown{false};
  bool fontFormIsShown{false};
  bool booksRefreshNeeded{false};

  #if DATE_TIME_RTC
    bool dateTimeFormIsShown{false};
  #endif
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    bool calibrationIsShown{false};
  #endif

  bool waitForKeyAfterWifi{false};
  bool webServerWasStarted{false};

  MenuViewerPtr menuViewer;
  FormViewerPtr formViewer;

public:
  OptionController()  = default;
  ~OptionController() = default;

  auto inputEvent(const EventMgr::Event &event) -> void;
  auto enter() -> void;
  auto leave(bool goingToDeepSleep = false) -> void;
  auto setFontCount(uint8_t count) -> void;

  inline auto setMainFormIsShown() -> void { mainFormIsShown = true; }
  inline auto setFontFormIsShown() -> void { fontFormIsShown = true; }

  #if DATE_TIME_RTC
    inline auto setDateTimeFormIsShown() -> void { dateTimeFormIsShown = true; }
  #endif

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    inline auto setCalibrationIsShown() -> void { calibrationIsShown = true; }
  #endif

  inline auto setWaitForKeyAfterWifi(bool webServerStarted = false) -> void {
    waitForKeyAfterWifi = true;
    webServerWasStarted = webServerStarted;
    mainFormIsShown     = false;
    fontFormIsShown     = false;
    #if DATE_TIME_RTC
      dateTimeFormIsShown = false;
    #endif
    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
      calibrationIsShown = false;
    #endif
  }

  auto mainParameters() -> void;
  auto defaultParameters() -> void;
  auto wifiMode() -> void;
  auto initNvs() -> void;
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    auto calibrate() -> void;
  #endif
  #if DATE_TIME_RTC
    auto clockAdjustForm() -> void;
    auto setClock() -> void;
    auto ntpClockAdjust() -> void;
  #endif
  #if DEBUGGING
    auto debugging() -> void;
  #endif
};

#if __OPTION_CONTROLLER__
  OptionController optionController;
#else
  extern OptionController optionController;
#endif
