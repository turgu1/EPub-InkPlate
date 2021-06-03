// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "screen.hpp"

#if defined(INKPLATE_6PLUS)
  #include "touch_screen.hpp"
#endif

class EventMgr
{
  private:
    bool stay_on;
    #if defined(INKPLATE_6PLUS) || TOUCH_TRIAL
      enum class State { NONE, HOLDING, TAPING, SWIPING, PINCHING };
      uint16_t x_pos,   y_pos;
      uint16_t x_start, y_start;
      State    state;
    #endif

  public:
    static constexpr char const * TAG = "EventMgr";

    #if defined(INKPLATE_6PLUS) || TOUCH_TRIAL
      enum class         KeyEvent { NONE, TAP,    HOLD,   SWIPE_LEFT, SWIPE_RIGHT, PINCH_ENLARGE, PINCH_REDUCE, RELEASE };
      enum class       InputEvent { NONE, PRESS1, PRESS2, MOVE,       RELEASE };

      const char * event_str[8] = { "NONE", "TAP", "HOLD", "SWIPE_LEFT", "SWIPE_RIGHT", "PINCH_ENLARGE", "PINCH_REDUCE", "RELEASE" };

      void           get_location(uint16_t & x, uint16_t & y) { x = x_pos;   y = y_pos;   }
      void     get_start_location(uint16_t & x, uint16_t & y) { x = x_start; y = y_start; }
      uint16_t     get_pinch_size();
      void            input_event(InputEvent event, uint16_t x, uint16_t y, bool hold);
    #else
      enum class KeyEvent { NONE, NEXT, PREV, DBL_NEXT, DBL_PREV, SELECT, DBL_SELECT };
    #endif

    EventMgr() : stay_on(false)
      #if defined(INKPLATE_6PLUS) || TOUCH_TRIAL
        , state(State::NONE)
      #endif
      { }

    bool setup();
    
    void loop();

    KeyEvent get_key();
    
    #if EPUB_LINUX_BUILD
      #if TOUCH_TRIAL
        //void input_event();
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

