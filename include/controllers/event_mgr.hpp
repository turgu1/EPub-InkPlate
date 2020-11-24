// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __EVENT_MGR_HPP__
#define __EVENT_MGR_HPP__

#include "global.hpp"

#include "screen.hpp"

class EventMgr
{
  private:
    bool stay_on;

  public:
    static constexpr char const * TAG = "EventMgr";

    enum KeyEvent { KEY_NONE, KEY_NEXT, KEY_PREV, KEY_DBL_NEXT, KEY_DBL_PREV, KEY_SELECT, KEY_DBL_SELECT };
    EventMgr() : stay_on(false) { }

    bool setup();
    
    void loop();

    KeyEvent get_key();
    
    void left();
    void right();
    void up();
    void down();
    void select();
    void home();

    inline void set_stay_on(bool value) { stay_on = value; };
    void set_orientation(Screen::Orientation orient);
};

#if __EVENT_MGR__
  EventMgr event_mgr;
#else
  extern EventMgr event_mgr;
#endif

#endif
