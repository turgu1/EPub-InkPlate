// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.
//
// This is the event manageer for linux base development.

#if (EPUB_LINUX_BUILD && !TOUCH_TRIAL)

  #define __EVENT_MGR__ 1
  #include "controllers/event_mgr.hpp"

  #include "config.hpp"
  #include "controllers/app_controller.hpp"

  #include <iostream>

  #include "screen.hpp"


  #include <gtk/gtk.h>

  #include "event_mgr.hpp"
  #include "screen.hpp"

  auto EventMgr::left() -> void {
    Event event;
    event.kind = EventKind::PREV;
    appController.inputEvent(event);
    appController.launch();
  }
  auto EventMgr::right() -> void {
    Event event;
    event.kind = EventKind::NEXT;
    appController.inputEvent(event);
    appController.launch();
  }
  auto EventMgr::up() -> void {
    Event event;
    event.kind = EventKind::DBL_PREV;
    appController.inputEvent(event);
    appController.launch();
  }
  auto EventMgr::down() -> void {
    Event event;
    event.kind = EventKind::DBL_NEXT;
    appController.inputEvent(event);
    appController.launch();
  }
  auto EventMgr::select() -> void {
    Event event;
    event.kind = EventKind::SELECT;
    appController.inputEvent(event);
    appController.launch();
  }
  auto EventMgr::home() -> void {
    Event event;
    event.kind = EventKind::DBL_SELECT;
    appController.inputEvent(event);
    appController.launch();
  }

  #define BUTTON_EVENT(button, msg)                                                                  \
          static void button ## _clicked(GObject *button, GParamSpec *property, gpointer data) {             \
            eventMgr.button();                                                                             \
          }

  BUTTON_EVENT(left,   "Left Clicked")
  BUTTON_EVENT(right,  "Right Clicked")
  BUTTON_EVENT(up,     "Up Clicked")
  BUTTON_EVENT(down,   "Down Clicked")
  BUTTON_EVENT(select, "Select Clicked")
  BUTTON_EVENT(home,   "Home Clicked")

  auto EventMgr::someEventWaiting() -> bool { return false; }

  auto EventMgr::loop() -> void {
    gtk_main();   // never return
  }

  auto EventMgr::setOrientation(Screen::Orientation orient) -> void {
    // Nothing to do...
  }

  auto EventMgr::setup() -> bool {

    g_signal_connect(G_OBJECT(screen.leftButton), "clicked", G_CALLBACK(left_clicked),
                     (gpointer)screen.window);
    g_signal_connect(G_OBJECT(screen.rightButton), "clicked", G_CALLBACK(right_clicked),
                     (gpointer)screen.window);
    g_signal_connect(G_OBJECT(screen.upButton), "clicked", G_CALLBACK(up_clicked),
                     (gpointer)screen.window);
    g_signal_connect(G_OBJECT(screen.downButton), "clicked", G_CALLBACK(down_clicked),
                     (gpointer)screen.window);
    g_signal_connect(G_OBJECT(screen.selectButton), "clicked", G_CALLBACK(select_clicked),
                     (gpointer)screen.window);
    g_signal_connect(G_OBJECT(screen.homeButton), "clicked", G_CALLBACK(home_clicked),
                     (gpointer)screen.window);

    return true;
  }

#endif
