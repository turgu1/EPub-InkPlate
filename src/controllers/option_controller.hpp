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
  OptionController() = default;

  void inputEvent(const EventMgr::Event &event);
  void enter();
  void leave(bool goingToDeepSleep = false);
  void setFontCount(uint8_t count);

  inline void setMainFormIsShown() { mainFormIsShown = true; }
  inline void setFontFormIsShown() { fontFormIsShown = true; }

  #if DATE_TIME_RTC
    inline void setDateTimeFormIsShown() { dateTimeFormIsShown = true; }
  #endif

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    inline void setCalibrationIsShown() { calibrationIsShown = true; }
  #endif

  inline void setWaitForKeyAfterWifi(bool webServerStarted = false) {
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

  void mainParameters();
  void defaultParameters();
  void wifiMode();
  void initNvs();
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    void calibrate();
  #endif
  #if DATE_TIME_RTC
    void clockAdjustForm();
    void setClock();
    void ntpClockAdjust();
  #endif
  #if DEBUGGING
    void debugging();
  #endif
};

#if __OPTION_CONTROLLER__
  OptionController optionController;
#else
  extern OptionController optionController;
#endif
