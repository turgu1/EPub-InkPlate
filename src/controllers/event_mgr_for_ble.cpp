// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.
//
// This is the event manageer for all kind of devices that use a BLE Keypad.
// For now, only the 6_V2 and the 10_V2 are supported.

#if (INKPLATE_6_V2 || INKPLATE_10_V2) && BLE_KEYPAD

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

  static QueueHandle_t touchpadIsrQueue   = NULL;
  static QueueHandle_t touchpadEventQueue = NULL;

  static QueueHandle_t bleKeypadEventQueue = NULL;
  auto EventMgr::getBLEKeypadEventQueue() -> QueueHandle_t { return bleKeypadEventQueue; }

  static void IRAM_ATTR touchpadIsrHandler(void *arg) {
    uint32_t gpioNum = (uint32_t)arg;
    xQueueSendFromISR(touchpadIsrQueue, &gpioNum, NULL);
  }

  uint8_t NEXT_PAD;
  uint8_t PREV_PAD;
  uint8_t SELECT_PAD;
  uint8_t DNEXT_PAD;
  uint8_t DPREV_PAD;
  uint8_t HOME_PAD;

  void getEventTask(void *param) {
    EventMgr::Event event;
    uint32_t        ioNum;
    uint8_t         pads;

    while (true) {

      event.kind = EventMgr::EventKind::NONE;

      // In BLE_KEYPAD mode, events are generated in the BLE GAP callback and sent to the same queue.
      xQueueReceive(bleKeypadEventQueue, &event, portMAX_DELAY);
      if (event.kind != EventMgr::EventKind::NONE) {
        xQueueSend(touchpadEventQueue, &event, 0);
      }
    }
  }

  auto EventMgr::setOrientation(Screen::Orientation orient) -> void {
    // Nothing to do
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
          #if BLE_KEYPAD
          #else
            int8_t lightSleepDuration = 5;
            config.get(Config::Ident::TIMEOUT, &lightSleepDuration);

            LOG_I("Light Sleep for {} minutes...", lightSleepDuration);
            ESP::delay(500);

            #if EXTENDED_CASE
              #define INT_PIN PressKeys::INTERRUPT_PIN
            #else
              #define INT_PIN TouchKeys::INTERRUPT_PIN
            #endif

            if (inkplate_platform.light_sleep(lightSleepDuration, INT_PIN, 1)) {

              LOG_D("Timed out on Light Sleep. Going now to Deep Sleep");

              gotoDeepSleep(lightSleepDuration);
            }
          #endif
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
    bleKeypadEventQueue = xQueueCreate(     // create a queue to handle key event from BLE GAP callback
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
