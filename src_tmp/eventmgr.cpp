#include <iostream>

#define __EVENT_MGR__ 1
#include "eventmgr.hpp"

#include "app_controller.hpp"

#if EPUB_LINUX_BUILD
  #include <gtk/gtk.h>
#endif

// static const char * TAG = "EventMgr";

EventMgr::EventMgr()
{
  
}

void EventMgr::start_loop()
{
  #if EPUB_LINUX_BUILD
    gtk_main();
  #endif
}

void EventMgr::left()   { app_controller.key_event(KEY_LEFT);   }
void EventMgr::right()  { app_controller.key_event(KEY_RIGHT);  }
void EventMgr::up()     { app_controller.key_event(KEY_UP);     }
void EventMgr::down()   { app_controller.key_event(KEY_DOWN);   }
void EventMgr::select() { app_controller.key_event(KEY_SELECT); }
void EventMgr::home()   { app_controller.key_event(KEY_HOME);   }
