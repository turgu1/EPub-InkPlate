// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#if defined(INKPLATE_6) || defined(INKPLATE_6_EXTENDED) || defined(INKPLATE_10) || defined(INKPLATE_10_EXTENDED) || (EPUB_LINUX_BUILD && !TOUCH_TRIAL)

#define __EVENT_MGR__ 1
#include "controllers/event_mgr.hpp"

#include "controllers/app_controller.hpp"
#include "models/config.hpp"

#include <iostream>

#if EPUB_INKPLATE_BUILD

  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
  #include "driver/gpio.h"

  #include "wire.hpp"
  #include "inkplate_platform.hpp"
  #include "viewers/msg_viewer.hpp"

  #if EXTENDED_CASE
    #include "press_keys.hpp"
  #else
    #include "touch_keys.hpp"
  #endif

  static xQueueHandle touchpad_isr_queue   = NULL;
  static xQueueHandle touchpad_event_queue = NULL;

  static void IRAM_ATTR 
  touchpad_isr_handler(void * arg)
  {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(touchpad_isr_queue, &gpio_num, NULL);
  }

  #if EXTENDED_CASE
    uint8_t   NEXT_PAD;
    uint8_t   PREV_PAD;
    uint8_t SELECT_PAD;
    uint8_t  DNEXT_PAD;
    uint8_t  DPREV_PAD;
    uint8_t   HOME_PAD;

    void
    get_event_task(void * param)
    {
      EventMgr::Event event;
      uint32_t io_num;
      uint8_t  pads;

      while (true) {
      
        event = EventMgr::Event::NONE;

        xQueueReceive(touchpad_isr_queue, &io_num, portMAX_DELAY);

        // An event interrupt happened. Retrieve pads information.
        // t1 = esp_timer_get_time();
        if ((pads = press_keys.read_all_keys()) == 0) {
          // Not fast enough or not synch with start of key strucked. Re-activating interrupts...
          Wire::enter();
          mcp_int.get_int_state();
          Wire::leave();  
        }
        else {
          // Wait until there is no key
          while (press_keys.read_all_keys() != 0) taskYIELD(); 

          // Re-activating interrupts.
          Wire::enter();
          mcp_int.get_int_state();
          Wire::leave();  

          if      (pads & SELECT_PAD) event = EventMgr::Event::SELECT;
          else if (pads & NEXT_PAD  ) event = EventMgr::Event::NEXT;
          else if (pads & PREV_PAD  ) event = EventMgr::Event::PREV;
          else if (pads & HOME_PAD  ) event = EventMgr::Event::DBL_SELECT;
          else if (pads & DNEXT_PAD ) event = EventMgr::Event::DBL_NEXT;
          else if (pads & DPREV_PAD ) event = EventMgr::Event::DBL_PREV;
        }

        if (event != EventMgr::Event::NONE) {
          xQueueSend(touchpad_event_queue, &event, 0);
        }  
      }     
    }

    void
    EventMgr::set_orientation(Screen::Orientation orient)
    {
      if (orient == Screen::Orientation::LEFT) {
          NEXT_PAD = (1 << (uint8_t)PressKeys::Key::U4);
          PREV_PAD = (1 << (uint8_t)PressKeys::Key::U3);
        SELECT_PAD = (1 << (uint8_t)PressKeys::Key::U1);
         DNEXT_PAD = (1 << (uint8_t)PressKeys::Key::U5);
         DPREV_PAD = (1 << (uint8_t)PressKeys::Key::U2);
          HOME_PAD = (1 << (uint8_t)PressKeys::Key::U6);
      } 
      else if (orient == Screen::Orientation::RIGHT) {
          NEXT_PAD = (1 << (uint8_t)PressKeys::Key::U3);
          PREV_PAD = (1 << (uint8_t)PressKeys::Key::U4);
        SELECT_PAD = (1 << (uint8_t)PressKeys::Key::U6);
         DNEXT_PAD = (1 << (uint8_t)PressKeys::Key::U2);
         DPREV_PAD = (1 << (uint8_t)PressKeys::Key::U5);
          HOME_PAD = (1 << (uint8_t)PressKeys::Key::U1);
      } 
      else {
          NEXT_PAD = (1 << (uint8_t)PressKeys::Key::U5);
          PREV_PAD = (1 << (uint8_t)PressKeys::Key::U2);
        SELECT_PAD = (1 << (uint8_t)PressKeys::Key::U6);
         DNEXT_PAD = (1 << (uint8_t)PressKeys::Key::U3);
         DPREV_PAD = (1 << (uint8_t)PressKeys::Key::U4);
          HOME_PAD = (1 << (uint8_t)PressKeys::Key::U1);
      }
    }
  #else
    uint8_t   NEXT_PAD;
    uint8_t   PREV_PAD;
    uint8_t SELECT_PAD;

    void
    get_event_task(void * param)
    {
      EventMgr::Event event;
      uint32_t io_num;
      uint8_t  pads, pads2;

      while (true) {
      
        event = EventMgr::Event::NONE;

        xQueueReceive(touchpad_isr_queue, &io_num, portMAX_DELAY);

        // An event interrupt happened. Retrieve pads information.
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
          while (xQueueReceive(touchpad_isr_queue, &io_num, pdMS_TO_TICKS(400))) {
            if ((pads2 = touch_keys.read_all_keys()) != 0) {
              found = true;
              break;
            }

            // There was no key, re-activate interrupts
            Wire::enter();
            mcp_int.get_int_state();
            Wire::leave();  
          }
          // t2 = esp_timer_get_time();

          if (found) {
            
            // Double Click on a key

            while (touch_keys.read_all_keys() != 0) taskYIELD();

            // Re-activating interrupts
            Wire::enter();
            mcp_int.get_int_state();
            Wire::leave();  

            if      (pads2 & SELECT_PAD) event = EventMgr::Event::DBL_SELECT;
            else if (pads2 & NEXT_PAD  ) event = EventMgr::Event::DBL_NEXT;
            else if (pads2 & PREV_PAD  ) event = EventMgr::Event::DBL_PREV;
          }
          else {

            // Simple Click on a key

            if      (pads & SELECT_PAD) event = EventMgr::Event::SELECT;
            else if (pads & NEXT_PAD  ) event = EventMgr::Event::NEXT;
            else if (pads & PREV_PAD  ) event = EventMgr::Event::PREV;
          }
        }

        if (event != EventMgr::Event::NONE) {
          xQueueSend(touchpad_event_queue, &event, 0);
        }  
      }     
    }

    void
    EventMgr::set_orientation(Screen::Orientation orient)
    {
      if (orient == Screen::Orientation::LEFT) {
          NEXT_PAD = 2;
          PREV_PAD = 1;
        SELECT_PAD = 4;
      } 
      else if (orient == Screen::Orientation::RIGHT) {
          NEXT_PAD = 2;
          PREV_PAD = 4;
        SELECT_PAD = 1;
      } 
      else {
          NEXT_PAD = 4;
          PREV_PAD = 2;
        SELECT_PAD = 1;
      }
    }
  #endif

  EventMgr::Event 
  EventMgr::get_event() 
  {
    Event event;
    if (xQueueReceive(touchpad_event_queue, &event, pdMS_TO_TICKS(15E3))) {
      return event;
    }
    else {
      return Event::NONE;
    }
  }

#else

  #include "screen.hpp"

#endif

#if EPUB_LINUX_BUILD

  #include <gtk/gtk.h>

  #include "screen.hpp"

  void EventMgr::left()   { app_controller.input_event(Event::PREV);       app_controller.launch(); }
  void EventMgr::right()  { app_controller.input_event(Event::NEXT);       app_controller.launch(); }
  void EventMgr::up()     { app_controller.input_event(Event::DBL_PREV);   app_controller.launch(); }
  void EventMgr::down()   { app_controller.input_event(Event::DBL_NEXT);   app_controller.launch(); }
  void EventMgr::select() { app_controller.input_event(Event::SELECT);     app_controller.launch(); }
  void EventMgr::home()   { app_controller.input_event(Event::DBL_SELECT); app_controller.launch(); }

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
    LOG_D("===> Loop...");
    while (1) {
      EventMgr::Event event;

      if ((event = get_event()) != Event::NONE) {
        LOG_D("Got event %d", (int)event);
        app_controller.input_event(event);
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

          #if EXTENDED_CASE
            #define INT_PIN PressKeys::INTERRUPT_PIN
          #else
            #define INT_PIN TouchKeys::INTERRUPT_PIN
          #endif
          
          if (inkplate_platform.light_sleep(light_sleep_duration, INT_PIN, 1)) {

            app_controller.going_to_deep_sleep();
            
            LOG_D("Timed out on Light Sleep. Going now to Deep Sleep");
            screen.force_full_update();
            msg_viewer.show(
              MsgViewer::INFO, 
              false, true, 
              "Deep Sleep", 
              "Timeout period exceeded (%d minutes). The device is now "
              "entering into Deep Sleep mode. Please press a key to restart.",
              light_sleep_duration);
            ESP::delay(1000);
            inkplate_platform.deep_sleep(INT_PIN, 1);
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
    g_signal_connect(G_OBJECT(  screen.left_button), "clicked", G_CALLBACK(  left_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT( screen.right_button), "clicked", G_CALLBACK( right_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(    screen.up_button), "clicked", G_CALLBACK(    up_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(  screen.down_button), "clicked", G_CALLBACK(  down_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(screen.select_button), "clicked", G_CALLBACK(select_clicked), (gpointer) screen.window);
    g_signal_connect(G_OBJECT(  screen.home_button), "clicked", G_CALLBACK(  home_clicked), (gpointer) screen.window);

  #else

    gpio_config_t io_conf;

    io_conf.intr_type    = GPIO_INTR_POSEDGE;   // Interrupt of rising edge
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_34; // Bit mask of the pin, use GPIO34 here
    io_conf.mode         = GPIO_MODE_INPUT;     // Set as input mode
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;  // Enable pull-up mode

    gpio_config(&io_conf);
    
    touchpad_isr_queue = xQueueCreate(          //create a queue to handle gpio event from isr
      10, sizeof(uint32_t));
    touchpad_event_queue = xQueueCreate(          //create a queue to handle key event from task
      10, sizeof(EventMgr::Event));

    TaskHandle_t xHandle = nullptr;
    xTaskCreate(get_event_task, "GetEvent", 2000, nullptr, 10, &xHandle);

    gpio_install_isr_service(0);                //install gpio isr service
    
    gpio_isr_handler_add(                       //hook isr handler for specific gpio pin
      GPIO_NUM_34, 
      touchpad_isr_handler, 
      (void *) GPIO_NUM_34);

    Wire::enter();
    mcp_int.get_int_state();                        // This is activating interrupts...
    Wire::leave();
  #endif

  return true;
}

#endif
