// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "screen.hpp"

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2
  #include "touch_screen.hpp"
  #include "inkplate_platform.hpp"
#endif

class EventMgr
{
  public:
    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || TOUCH_TRIAL
      struct CalibPoint {
        uint16_t x[3], y[3];
      };
      struct TouchPoint {
        uint16_t x[3], y[3];
      };
    #endif

  protected:
    bool stay_on;
    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || TOUCH_TRIAL
      
      int64_t    a, b, c, d, e, f, divider;
      
      CalibPoint calib_point;
      TouchPoint touch_point;
      uint8_t    calib_count;

      uint16_t x_pos, y_pos;
      uint16_t distance;

      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2
        void retrieve_calibration_values();
      #endif
    #endif

  public:
    static constexpr char const * TAG = "EventMgr";

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || TOUCH_TRIAL
      enum class EventKind { NONE,        TAP,           HOLD,         SWIPE_LEFT, 
                             SWIPE_RIGHT, PINCH_ENLARGE, PINCH_REDUCE, RELEASE      };

      static const char * event_str[8];

      struct Event {
        EventKind kind;
        uint16_t x, y, dist;
      };

      void show_calibration();
      bool calibration_event(const Event & event);
      void  set_position(uint16_t   x, uint16_t   y) { x_pos = x; y_pos = y; }
      void  get_position(uint16_t & x, uint16_t & y) { x = x_pos; y = y_pos; }
      void to_user_coord(uint16_t & x, uint16_t & y);

    #else
      enum class EventKind { NONE, NEXT, PREV, DBL_NEXT, DBL_PREV, SELECT, DBL_SELECT };

      struct Event {
        EventKind kind;
      };
    #endif

    
    EventMgr() : stay_on(false) { }

    bool setup();
    
    void loop();

    const Event & get_event();
    
    #if EPUB_LINUX_BUILD
      #if TOUCH_TRIAL
        //void low_input_event();
      #else
        void   left();
        void  right();
        void     up();
        void   down();
        void select();
        void   home();
      #endif    
    #endif

    inline void     set_stay_on(bool value) { stay_on = value; };
    void        set_orientation(Screen::Orientation orient);
};

#if __EVENT_MGR__
  EventMgr event_mgr;
#else
  extern EventMgr event_mgr;
#endif

