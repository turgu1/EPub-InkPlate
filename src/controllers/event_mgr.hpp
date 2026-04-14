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
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    struct CalibPoint {
      uint16_t x[3], y[3];
    };
    struct TouchPoint {
      uint16_t x[3], y[3];
    };
  #endif

protected:
  volatile bool stayOn{false};
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL

    int64_t a, b, c, d, e, f, divider;

    CalibPoint calibPoint;
    TouchPoint touchPoint;
    uint8_t calibCount;

    uint16_t xPos, yPos;
    uint16_t distance;

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
      void retrieveCalibrationValues();
    #endif
  #endif

public:
  static constexpr char const *TAG = "EventMgr";

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    // clang-format off
    enum class EventKind { NONE,        TAP,           HOLD,         SWIPE_LEFT, 
                           SWIPE_RIGHT, PINCH_ENLARGE, PINCH_REDUCE, RELEASE,
                           WAKEUP_BUTTON};
    // clang-format on

    static const char *eventStr[9];

    struct Event {
      EventKind kind;
      uint16_t x, y, dist;
    };

    void showCalibration();
    auto calibrationEvent(const Event &event) -> bool;
    void setPosition(uint16_t x, uint16_t y) {
      xPos = x;
      yPos = y;
    }
    void getPosition(uint16_t &x, uint16_t &y) {
      x = xPos;
      y = yPos;
    }
    void toUserCoord(uint16_t &x, uint16_t &y);

  #else
    enum class EventKind {
      NONE, NEXT, PREV, DBL_NEXT, DBL_PREV, SELECT, DBL_SELECT
    };

    struct Event {
      EventKind kind;
    };
  #endif

  EventMgr() = default;

  auto setup() -> bool;

  void loop();

  auto getEvent() -> const Event &;

  #if EPUB_LINUX_BUILD
    #if TOUCH_TRIAL
      // void low_input_event();
    #else
      void left();
      void right();
      void up();
      void down();
      void select();
      void home();
    #endif
  #endif

  inline void setStayOn(bool value) { stayOn = value; };
  inline auto stayingOn() -> bool { return stayOn; };
  void setOrientation(Screen::Orientation orient);
};

#if __EVENT_MGR__
  EventMgr eventMgr;
#else
  extern EventMgr eventMgr;
#endif
