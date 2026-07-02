// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.
//
// This is the event manageer for the first devices mad by E-Radionica.
// They use 3 tactical buttons

#if (INKPLATE_6 || INKPLATE_10) && !(EXTENDED_CASE || BLE_KEYPAD)

  #define __EVENT_MGR__ 1
  #include "controllers/event_mgr.hpp"

  #include "config.hpp"
  #include "controllers/app_controller.hpp"

  #include <iostream>

  #include "driver/gpio.h"
  #include "freertos/FreeRTOS.h"
  #include "freertos/queue.h"
  #include "freertos/task.h"

  #include "helpers/goto_deep_sleep.hpp"
  #include "inkplate_platform.hpp"
  #include "models/page_locs.hpp"
  #include "viewers/msg_viewer.hpp"
  #include "viewers/screen_saver.hpp"
  #include "wire.hpp"

  #include "touch_keys.hpp"

  static QueueHandle_t touchpadIsrQueue   = NULL;
  static QueueHandle_t touchpadEventQueue = NULL;

  static void IRAM_ATTR touchpadIsrHandler(void *arg) {
    uint32_t gpioNum = (uint32_t)arg;
    xQueueSendFromISR(touchpadIsrQueue, &gpioNum, NULL);
  }

  uint8_t NEXT_PAD;
  uint8_t PREV_PAD;
  uint8_t SELECT_PAD;

  void getEventTask(void *param) {
    EventMgr::Event event;
    uint32_t        ioNum;
    uint8_t         pads, pads2;

    while (true) {

      event.kind = EventMgr::EventKind::NONE;

      xQueueReceive(touchpadIsrQueue, &ioNum, portMAX_DELAY);

      // An event interrupt happened. Retrieve pads information.
      // t1 = esp_timer_get_time();
      if ((pads = touch_keys.read_all_keys()) == 0) {
        // Not fast enough or not synch with start of key strucked. Re-activating interrupts...
        Wire::enter();
        io_expander_int.get_int_state();
        Wire::leave();
      } else {
        // Wait until there is no key
        while (touch_keys.read_all_keys() != 0) taskYIELD();

        // Re-activating interrupts.
        Wire::enter();
        io_expander_int.get_int_state();
        Wire::leave();

        // Wait for potential second key
        bool found = false;
        while (xQueueReceive(touchpadIsrQueue, &ioNum, pdMS_TO_TICKS(400))) {
          if ((pads2 = touch_keys.read_all_keys()) != 0) {
            found = true;
            break;
          }

          // There was no key, re-activate interrupts
          Wire::enter();
          io_expander_int.get_int_state();
          Wire::leave();
        }
        // t2 = esp_timer_get_time();

        if (found) {

          // Double Click on a key

          while (touch_keys.read_all_keys() != 0) taskYIELD();

          // Re-activating interrupts
          Wire::enter();
          io_expander_int.get_int_state();
          Wire::leave();

          if (pads2 & SELECT_PAD) {
            event.kind = EventMgr::EventKind::DBL_SELECT;
          }
          else if (pads2 & NEXT_PAD) {
            event.kind = EventMgr::EventKind::DBL_NEXT;
          }
          else if (pads2 & PREV_PAD) {
            event.kind = EventMgr::EventKind::DBL_PREV;
          }
        } else {

          // Simple Click on a key

          if (pads & SELECT_PAD) {
            event.kind = EventMgr::EventKind::SELECT;
          }
          else if (pads & NEXT_PAD) {
            event.kind = EventMgr::EventKind::NEXT;
          }
          else if (pads & PREV_PAD) {
            event.kind = EventMgr::EventKind::PREV;
          }
        }
      }

      if (event.kind != EventMgr::EventKind::NONE) {
        xQueueSend(touchpadEventQueue, &event, 0);
      }
    }
  }

  auto EventMgr::setOrientation(Screen::Orientation orient) -> void {
    if (orient == Screen::Orientation::LEFT) {
      NEXT_PAD   = 2;
      PREV_PAD   = 1;
      SELECT_PAD = 4;
    } else if (orient == Screen::Orientation::RIGHT) {
      NEXT_PAD   = 2;
      PREV_PAD   = 4;
      SELECT_PAD = 1;
    } else {
      NEXT_PAD   = 4;
      PREV_PAD   = 2;
      SELECT_PAD = 1;
    }
  }

  auto EventMgr::someEventWaiting() -> bool { return uxQueueMessagesWaiting(touchpadEventQueue) > 0; }

  auto EventMgr::getEvent() -> const EventMgr::Event & {
    static Event event;
    if (!xQueueReceive(touchpadEventQueue, &event, pdMS_TO_TICKS(15E3))) {
      event.kind = EventKind::NONE;
    }
    return event;
  }

  auto EventMgr::loop() -> void {
    LOG_D("===> Loop...");
    while (1) {
      const EventMgr::Event &event = getEvent();

      if (event.kind != EventKind::NONE) {
        LOG_D("Got event {}", (int)event.kind);
        appController.inputEvent(event);
        ESP::show_heaps_info();
        return;
      } else {
        // Nothing received in 15 seconds, put the device in Light Sleep Mode.
        // After some delay, the device will then be put in Deep Sleep Mode,
        // rebooting after the user press a key.

        if (!stayOn) {   // Unless somebody wants to keep us awake...
          if (pageLocs.isControlTaskReadyToBeStopped()) { pageLocs.stopControlTask(); }

          int8_t lightSleepDuration = 5;
          config.get(Config::Ident::TIMEOUT, &lightSleepDuration);

          LOG_I("Light Sleep for {} minutes...", lightSleepDuration);
          ESP::delay(500);

          if (inkplate_platform.light_sleep(lightSleepDuration, TouchKeys::INTERRUPT_PIN, 1)) {

            LOG_D("Timed out on Light Sleep. Going now to Deep Sleep");

            gotoDeepSleep(lightSleepDuration);
          }
        }
      }
    }
  }

  auto EventMgr::setup() -> bool {

    gpio_config_t io_conf;

    io_conf.intr_type    = GPIO_INTR_POSEDGE;  // Interrupt of rising edge
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_34;   // Bit mask of the pin, use GPIO34 here
    io_conf.mode         = GPIO_MODE_INPUT;   // Set as input mode
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;  // Disable pull-up mode (not available for GPIO34)

    gpio_config(&io_conf);

    touchpadIsrQueue   = xQueueCreate(  // create a queue to handle gpio event from isr
      10, sizeof(uint32_t));
    touchpadEventQueue = xQueueCreate(   // create a queue to handle key event from task
      10, sizeof(EventMgr::Event));

    TaskHandle_t xHandle = nullptr;
    xTaskCreate(getEventTask, "GetEvent", 2000, nullptr, 2 | portPRIVILEGE_BIT, &xHandle);

    gpio_install_isr_service(0);   // install gpio isr service

    gpio_isr_handler_add( // hook isr handler for specific gpio pin
      GPIO_NUM_34, touchpadIsrHandler, (void *)GPIO_NUM_34);

    Wire::enter();
    io_expander_int.get_int_state();   // This is activating interrupts...
    Wire::leave();

    return true;
  }

#endif
