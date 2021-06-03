// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#if defined(INKPLATE_6PLUS) || TOUCH_TRIAL

#include <iostream>

#define __EVENT_MGR__ 1
#include "controllers/event_mgr.hpp"

#include "controllers/app_controller.hpp"
#include "models/config.hpp"
#include "logging.hpp"

#if EPUB_INKPLATE_BUILD

  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
  #include "driver/gpio.h"

  #include "wire.hpp"
  #include "inkplate_platform.hpp"
  #include "viewers/msg_viewer.hpp"

  #include "touch_screen.hpp"

  static xQueueHandle touchscreen_evt_queue  = NULL;
  static xQueueHandle touchscreen_key_queue  = NULL;

  static void IRAM_ATTR 
  touchscreen_isr_handler(void * arg)
  {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(touchscreen_evt_queue, &gpio_num, NULL);
  }

  void
  get_key_task(void * param)
  {
    EventMgr::KeyEvent key;

    while (true) {
    
      key = EventMgr::KeyEvent::NONE;

      xQueueReceive(touchscreen_evt_queue, &io_num, portMAX_DELAY);

      // A key interrupt happened. Retrieve pads information.
      // t1 = esp_timer_get_time();
      if ((pads = touch_keys.read_all_keys()) == 0) {
        // Not fast enough or not synch with start of key strucked. Re-activating interrupts...
        Wire::enter();
        mcp_int.get_int_state();
        Wire::leave();  
      }
      else {
        // Wait until there is no key
        while (touch_keys.read_all_keys() != 0) taskYIELD(); 

        // Re-activating interrupts.
        Wire::enter();
        mcp_int.get_int_state();
        Wire::leave();  

        // Wait for potential second key
        bool found = false; 
        while (xQueueReceive(touchscreen_evt_queue, &io_num, pdMS_TO_TICKS(400))) {
          if ((pads2 = touch_keys.read_all_keys()) != 0) {
            found = true;
            break;
          }

          // There was no key, re-activate interrupts
          Wire::enter();
          mcp_int.get_int_state();
          Wire::leave();  
        }
      }

      if (key != EventMgr::KeyEvent::NONE) {
        xQueueSend(touchscreen_key_queue, &key, 0);
      }  
    }     
  }

  void
  EventMgr::set_orientation(Screen::Orientation orient)
  {
  }

  EventMgr::KeyEvent 
  EventMgr::get_key() 
  {
    KeyEvent key;
    if (xQueueReceive(touchscreen_key_queue, &key, pdMS_TO_TICKS(15E3))) {
      return key;
    }
    else {
      return KeyEvent::NONE;
    }
  }
#endif

#if EPUB_LINUX_BUILD

  #include <gtk/gtk.h>

  #include "screen.hpp"

  void 
  EventMgr::input_event(EventMgr::InputEvent event, uint16_t x, uint16_t y, bool hold)
  {
    KeyEvent ke = KeyEvent::NONE;

    if (event == InputEvent::PRESS1) {
      x_pos = x_start = x;
      y_pos = y_start = y;
      if (hold) {
        ke = KeyEvent::HOLD;
        state = State::HOLDING;
        // LOG_D("Holding...");
      }
      else {
        state = State::TAPING;
        // LOG_D("Taping...");
      }
    }
    else if (event == InputEvent::PRESS2) {
      x_pos = x_start = x;
      y_pos = y_start = y;
      state = State::PINCHING;
      // LOG_D("Pinching...")
    }
    else if (event == InputEvent::MOVE) {
      if (state == State::TAPING) {
        x_pos = x;
        y_pos = y;
        if (abs(((int16_t) x) - ((int16_t) x_start)) > 50) {
          state = State::SWIPING;
          // LOG_D("Swiping...");
        }
      }
      else if (state == State::PINCHING) {
        if (abs(((int16_t) x) - ((int16_t) x_pos)) > 20) {
          ke = (x < x_pos) ? KeyEvent::PINCH_REDUCE : KeyEvent::PINCH_ENLARGE;
          x_pos = x;
          y_pos = y;
        }
      }
    }
    else if (event == InputEvent::RELEASE) {
      x_pos = x;
      y_pos = y;
      if (state == State::TAPING) {
        ke = KeyEvent::TAP;
      }
      else if (state == State::SWIPING) {
        ke = (x < x_start) ? KeyEvent::SWIPE_LEFT : KeyEvent::SWIPE_RIGHT;
      }
      else { // PINCHING or HOLDING
        ke = KeyEvent::RELEASE;
      }
      state = State::NONE;
      // LOG_D("None...");
    }
    if (ke != KeyEvent::NONE) {
      LOG_D("Input Event %s [%d, %d] [%d, %d]...", event_str[int(ke)], x_start, y_start, x_pos, y_pos);
      app_controller.key_event(ke);
      app_controller.launch();
    }
  }

  static gboolean
  mouse_event_callback(GtkWidget * event_box,
                       GdkEvent  * event,
                       gpointer    data) 
  {
    EventMgr::InputEvent ev = EventMgr::InputEvent::NONE;
    uint16_t x, y;

    switch (event->type) {
      case GDK_BUTTON_RELEASE:
        x = ((GdkEventButton *) event)->x;
        y = ((GdkEventButton *) event)->y;
        ev = EventMgr::InputEvent::RELEASE;
        break;
      case GDK_BUTTON_PRESS:
        x = ((GdkEventButton *) event)->x;
        y = ((GdkEventButton *) event)->y;
        if (((GdkEventButton *) event)->button == 1) {
          ev = EventMgr::InputEvent::PRESS1;
        }
        else if (((GdkEventButton *) event)->button == 3) { // simulate pinch
          ev = EventMgr::InputEvent::PRESS2;
        }
        break;
      case GDK_MOTION_NOTIFY:
        x = ((GdkEventMotion *) event)->x;
        y = ((GdkEventMotion *) event)->y;
        ev = EventMgr::InputEvent::MOVE;
        break;
      default:
        break;
    }

    if (ev != EventMgr::InputEvent::NONE) {
      event_mgr.input_event(ev, x, y, ((GdkEventButton *) event)->state & GDK_CONTROL_MASK);
      return true;
    }
    else {
      return false;
    }
  }

  void EventMgr::loop()
  {
    gtk_main(); // never return
  }

  void
  EventMgr::set_orientation(Screen::Orientation orient)
  {
    // Nothing to do...
  }

#else
  void EventMgr::loop()
  {
    while (1) {
      EventMgr::KeyEvent key;

      if ((key = get_key()) != KeyEvent::NONE) {
        LOG_D("Got key %d", (int)key);
        app_controller.key_event(key);
        ESP::show_heaps_info();
        return;
      }
      else {
        // Nothing received in 15 seconds, put the device in Light Sleep Mode.
        // After some delay, the device will then be put in Deep Sleep Mode, 
        // rebooting after the user press a key.

        if (!stay_on) { // Unless somebody wants to keep us awake...
          int8_t light_sleep_duration;
          config.get(Config::Ident::TIMEOUT, &light_sleep_duration);

          LOG_I("Light Sleep for %d minutes...", light_sleep_duration);
          ESP::delay(500);


          if (inkplate_platform.light_sleep(light_sleep_duration, TouchScreen::INTERRUPT_PIN, 1)) {

            app_controller.going_to_deep_sleep();
            
            LOG_D("Timed out on Light Sleep. Going now to Deep Sleep");
            msg_viewer.show(
              MsgViewer::INFO, 
              false, true, 
              "Deep Sleep", 
              "Timeout period exceeded (%d minutes). The device is now "
              "entering into Deep Sleep mode. Please press the WakeUp Button to restart.",
              light_sleep_duration);
            ESP::delay(500);

            inkplate_platform.deep_sleep(TouchScreen::INTERRUPT_PIN, 1);
          }
        }
      }
    }
  }
#endif

bool
EventMgr::setup()
{
  #if EPUB_LINUX_BUILD

    g_signal_connect(G_OBJECT (screen.image_box),
                     "event",
                     G_CALLBACK (mouse_event_callback),
                     screen.get_image());
  #else
    
    touchscreen_evt_queue = xQueueCreate(          //create a queue to handle gpio event from isr
      10, sizeof(uint32_t));
    touchscreen_key_queue = xQueueCreate(          //create a queue to handle key event from task
      10, sizeof(EventMgr::KeyEvent));

    TaskHandle_t xHandle = NULL;
    xTaskCreate(get_key_task, "GetKey", 2000, nullptr, 10, &xHandle);

  #endif

  return true;
}

#endif
