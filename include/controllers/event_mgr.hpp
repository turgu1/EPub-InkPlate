// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __EVENT_MGR_HPP__
#define __EVENT_MGR_HPP__

#include "global.hpp"

class EventMgr
{
  public:
    static constexpr char const * TAG = "EventMgr";

    enum KeyEvent { KEY_NONE, KEY_NEXT, KEY_PREV, KEY_DBL_NEXT, KEY_DBL_PREV, KEY_SELECT, KEY_DBL_SELECT };
    EventMgr();

    bool setup();
    void start_loop();

    KeyEvent get_key();
    
    void left();
    void right();
    void up();
    void down();
    void select();
    void home();
};

#if __EVENT_MGR__
  EventMgr event_mgr;
#else
  extern EventMgr event_mgr;
#endif

#endif
