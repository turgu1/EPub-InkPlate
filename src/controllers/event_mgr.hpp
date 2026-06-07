// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "screen.hpp"

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
  #if INKPLATE_6FLICK
    #include "touch_screen_cypress.hpp"
  #else
    #include "touch_screen_elan.hpp"
  #endif
  #include "inkplate_platform.hpp"
#endif

class EventMgr {
  public:
    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL || TOUCH_MENU
      struct CalibPoint {
        uint16_t x[3], y[3];
      };
      struct TouchPoint {
        uint16_t x[3], y[3];
      };
    #endif

  protected:
    volatile bool stayOn{ false };

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL || TOUCH_MENU

      int64_t a, b, c, d, e, f, divider;

      CalibPoint calibPoint;
      TouchPoint touchPoint;
      uint8_t calibCount;

      uint16_t xPos, yPos;
      uint16_t distance;

      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
        auto retrieveCalibrationValues() -> void;
      #endif
    #endif

  public:
    static constexpr char const *TAG = "EventMgr";

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL || TOUCH_MENU
      enum class EventKind {
        NONE,         TAP,     HOLD,          SWIPE_LEFT, SWIPE_RIGHT, PINCH_ENLARGE,
        PINCH_REDUCE, RELEASE, WAKEUP_BUTTON,
        #if EPUB_LINUX_BUILD
          NEXT, PREV, DBL_NEXT, DBL_PREV, SELECT,  DBL_SELECT
        #endif
      };

      static const char *eventStr[9];

      struct Event {
        EventKind kind;
        uint16_t x, y, dist;
      };

      auto showCalibration() -> void;
      auto calibrationEvent(const Event &event) -> bool;
      auto setPosition(uint16_t x, uint16_t y) -> void {
        xPos = x;
        yPos = y;
      }
      auto getPosition(uint16_t &x, uint16_t &y) -> void {
        x = xPos;
        y = yPos;
      }
      auto toUserCoord(uint16_t &x, uint16_t &y) -> void;

    #else
      enum class EventKind { NONE, NEXT, PREV, DBL_NEXT, DBL_PREV, SELECT, DBL_SELECT };

      struct Event {
        EventKind kind;
      };

      #if BLE_KEYPAD
        auto getBLEKeypadEventQueue() -> QueueHandle_t;
      #endif
    #endif

    EventMgr()  = default;
    ~EventMgr() = default;

    auto setup() -> bool;

    auto loop() -> void;

    auto someEventWaiting() -> bool;

    auto getEvent() -> const Event &;

    #if EPUB_LINUX_BUILD
      #if TOUCH_TRIAL
        // void low_input_event();
      #else
        auto left() -> void;
        auto right() -> void;
        auto up() -> void;
        auto down() -> void;
        auto select() -> void;
        auto home() -> void;
      #endif
    #endif

    inline auto setStayOn(bool value) -> void { stayOn = value; };
    [[nodiscard]] inline auto stayingOn() -> bool { return stayOn; };
    auto setOrientation(Screen::Orientation orient) -> void;
};

#if __EVENT_MGR__
  EventMgr eventMgr;
#else
  extern EventMgr eventMgr;
#endif
