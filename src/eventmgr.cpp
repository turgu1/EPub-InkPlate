#include <iostream>

#define __EVENT_MGR__ 1
#include "eventmgr.hpp"
#include "app_controller.hpp"

// static const char * TAG = "EventMgr";

EventMgr::EventMgr()
{
  
}

#if EPUB_LINUX_BUILD

  #include <gtk/gtk.h>

  #include "screen.hpp"

  void EventMgr::left()   { app_controller.key_event(KEY_LEFT);   }
  void EventMgr::right()  { app_controller.key_event(KEY_RIGHT);  }
  void EventMgr::up()     { app_controller.key_event(KEY_UP);     }
  void EventMgr::down()   { app_controller.key_event(KEY_DOWN);   }
  void EventMgr::select() { app_controller.key_event(KEY_SELECT); }
  void EventMgr::home()   { app_controller.key_event(KEY_HOME);   }

  #define BUTTON_EVENT(button, msg) \
    static void button##_clicked(GObject * button, GParamSpec * property, gpointer data) { \
      event_mgr.button(); \
    }

  BUTTON_EVENT(left,   "Left Clicked"  )
  BUTTON_EVENT(right,  "Right Clicked" )
  BUTTON_EVENT(up,     "Up Clicked"    )
  BUTTON_EVENT(down,   "Down Clicked"  )
  BUTTON_EVENT(select, "Select Clicked")
  BUTTON_EVENT(home,   "Home Clicked"  )

  void EventMgr::start_loop()
  {
    g_signal_connect(G_OBJECT(  screen.left_button), "clicked", G_CALLBACK(  left_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT( screen.right_button), "clicked", G_CALLBACK( right_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(    screen.up_button), "clicked", G_CALLBACK(    up_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(  screen.down_button), "clicked", G_CALLBACK(  down_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(screen.select_button), "clicked", G_CALLBACK(select_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(  screen.home_button), "clicked", G_CALLBACK(  home_clicked), (gpointer) screen.window);

    gtk_main();
  }

#else
  void EventMgr::start_loop()
  {
  }
#endif