// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include <iostream>

#define __EVENT_MGR__ 1
#include "controllers/event_mgr.hpp"

#include "controllers/app_controller.hpp"
#include "models/config.hpp"
#include "logging.hpp"

#if EPUB_INKPLATE6_BUILD

  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
  #include "driver/gpio.h"
  #include "mcp.hpp"
  #include "inkplate6_ctrl.hpp"
  #include "viewers/msg_viewer.hpp"

  #define  TOUCHPAD_LEFT_POSITION 1
  #define TOUCHPAD_RIGHT_POSITION 0
  #define  TOUCHPAD_DOWN_POSITION 0

  #if TOUCHPAD_LEFT_POSITION
    constexpr uint8_t   NEXT_PAD = 2;
    constexpr uint8_t   PREV_PAD = 1;
    constexpr uint8_t SELECT_PAD = 4;
  #endif
  #if TOUCHPAD_RIGHT_POSITION
    constexpr uint8_t   NEXT_PAD = 2;
    constexpr uint8_t   PREV_PAD = 4;
    constexpr uint8_t SELECT_PAD = 1;
  #endif
  #if TOUCHPAD_DOWN_POSITION
    constexpr uint8_t   NEXT_PAD = 4;
    constexpr uint8_t   PREV_PAD = 2;
    constexpr uint8_t SELECT_PAD = 1;
  #endif

  static xQueueHandle touchpad_evt_queue  = NULL;

  static void IRAM_ATTR 
  touchpad_isr_handler(void * arg)
  {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(touchpad_evt_queue, &gpio_num, NULL);
  }

  EventMgr::KeyEvent 
  EventMgr::get_key()
  {
    KeyEvent key;
    uint32_t io_num;
    uint8_t  pads, pads2;

    key = KEY_NONE;

    if (xQueueReceive(touchpad_evt_queue, &io_num, pdMS_TO_TICKS(15E3))) {
      if ((pads = inkplate6_ctrl.read_touchpads()) == 0) {
        mcp.get_int_state();                     // This is re-activating interrupts...
      }
      else {
        while (inkplate6_ctrl.read_touchpads() != 0) taskYIELD();
        //ESP::delay_microseconds(20E3); //Wait 20 millis...

        mcp.get_int_state();                       // This is re-activating interrupts...

        bool found = false;
        while (xQueueReceive(touchpad_evt_queue, &io_num, pdMS_TO_TICKS(500))) {
          if ((pads2 = inkplate6_ctrl.read_touchpads()) != 0) {
            found = true;
            break;
          }
          mcp.get_int_state();
        }
        if (found) {
          
          // Double Click on a key

          while (inkplate6_ctrl.read_touchpads() != 0) taskYIELD();

          mcp.get_int_state();                     // This is re-activating interrupts...

          if      (pads2 & SELECT_PAD) key = KEY_DBL_SELECT;
          else if (pads2 & NEXT_PAD  ) key = KEY_DBL_NEXT;
          else if (pads2 & PREV_PAD  ) key = KEY_DBL_PREV;
        }
        else {

          // Simple Click on a key

          if      (pads & SELECT_PAD) key = KEY_SELECT;
          else if (pads & NEXT_PAD  ) key = KEY_NEXT;
          else if (pads & PREV_PAD  ) key = KEY_PREV;
        }
      }
    }     

    return key;
  }

#else

  #include "screen.hpp"

#endif

#if EPUB_LINUX_BUILD

  #include <gtk/gtk.h>

  #include "screen.hpp"

  void EventMgr::left()   { app_controller.key_event(KEY_PREV);       }
  void EventMgr::right()  { app_controller.key_event(KEY_NEXT);       }
  void EventMgr::up()     { app_controller.key_event(KEY_DBL_PREV);   }
  void EventMgr::down()   { app_controller.key_event(KEY_DBL_NEXT);   }
  void EventMgr::select() { app_controller.key_event(KEY_SELECT);     }
  void EventMgr::home()   { app_controller.key_event(KEY_DBL_SELECT); }

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
    gtk_main();
  }

#else
  void EventMgr::start_loop()
  {
    while (1) {
      EventMgr::KeyEvent key;

      if ((key = get_key()) != KEY_NONE) {
        LOG_I("Got key %d", key);
        app_controller.key_event(key);
      }
      else {
        // Nothing received in 5 seconds, put the device in Light Sleep Mode.
        // After some delay, the device will then be put in Deep Sleep Mode, 
        // rebooting after the user press a key.

        if (!stay_on) {
          int8_t light_sleep_duration;
          config.get(Config::TIMEOUT, light_sleep_duration);

          LOG_I("Light Sleep for %d minutes...", light_sleep_duration);
          ESP::delay(500);

          if (inkplate6_ctrl.light_sleep(light_sleep_duration)) {
            LOG_D("Timed out on Light Sleep. Going now to Deep Sleep");
            msg_viewer.show(
              MsgViewer::INFO, 
              false, true, 
              "Deep Sleep", 
              "Timeout period exceeded (%d minutes). The device is now "
              "entering into Deep Sleep mode. Please press a key to restart.",
              light_sleep_duration);
            ESP::delay(500);
            app_controller.going_to_deep_sleep();
            inkplate6_ctrl.deep_sleep();
          }
        }
      }
    }

    // Simple Uart based keyboard entry
    //
    //  a   s    d   f
    //  <<  <    >   >>
    //
    //  Select:        Space bar
    //  Double Select: Return
    // 
    // while(1) {
		//   uint8_t ch = fgetc(stdin);
	  //   if (ch != 0xFF) {
    //     switch (ch) {
    //       case 'a'  : app_controller.key_event(KEY_DBL_LEFT);   break;
    //       case 's'  : app_controller.key_event(KEY_LEFT);       break;
    //       case 'd'  : app_controller.key_event(KEY_PREV);       break;
    //       case 'f'  : app_controller.key_event(KEY_DBL_PREV);   break;
    //       case ' '  : app_controller.key_event(KEY_SELECT);     break;
    //       case '\r'  : app_controller.key_event(KEY_DBL_SELECT); break;
    //       default: 
    //         fputc('!', stdout);
    //         break;
    //     }
	  //   }
    // }  
  }
#endif

bool
EventMgr::setup()
{
  #if EPUB_LINUX_BUILD
    g_signal_connect(G_OBJECT(  screen.left_button), "clicked", G_CALLBACK(  left_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT( screen.right_button), "clicked", G_CALLBACK( right_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(    screen.up_button), "clicked", G_CALLBACK(    up_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(  screen.down_button), "clicked", G_CALLBACK(  down_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(screen.select_button), "clicked", G_CALLBACK(select_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(  screen.home_button), "clicked", G_CALLBACK(  home_clicked), (gpointer) screen.window);
  #else

    gpio_config_t io_conf;

    io_conf.intr_type    = GPIO_INTR_POSEDGE;    // Interrupt of rising edge
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_34;  // Bit mask of the pin, use GPIO34 here
    io_conf.mode         = GPIO_MODE_INPUT;      // Set as input mode
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;  // Disable pull-up mode

    gpio_config(&io_conf);
    
    touchpad_evt_queue = xQueueCreate(           //create a queue to handle gpio event from isr
      10, sizeof(uint32_t));

    gpio_install_isr_service(0);                 //install gpio isr service
    
    gpio_isr_handler_add(                        //hook isr handler for specific gpio pin
      GPIO_NUM_34, 
      touchpad_isr_handler, 
      (void *) GPIO_NUM_34);

    mcp.get_int_state();                         // This is activating interrupts...
  #endif

  return true;
}