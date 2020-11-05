#ifndef __EVENTMGR_HPP__
#define __EVENTMGR_HPP__

#include "global.hpp"

class EventMgr
{
  public:
    enum KeyEvent { KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_SELECT, KEY_HOME };
    EventMgr();

    void start_loop();

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
