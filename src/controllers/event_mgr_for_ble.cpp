// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.
//
// This is the event manageer for all kind of devices that use a BLE Keypad.
// For now, only the 6_V2 and the 10_V2 are supported.

#if (INKPLATE_6 || INKPLATE_10 || INKPLATE_5_V2 || INKPLATE_6_V2 || INKPLATE_10_V2) && BLE_KEYPAD

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
  #include "helpers/show_bt_state.hpp"
  #include "inkplate_platform.hpp"
  #include "models/page_locs.hpp"
  #include "viewers/msg_viewer.hpp"
  #include "viewers/screen_saver.hpp"
  #include "wire.hpp"

  #include "ble_keypad.hpp"

  static QueueHandle_t mainEventQueue = NULL;

  static QueueHandle_t bleKeypadEventQueue = NULL;
  auto EventMgr::getBLEKeypadEventQueue() -> QueueHandle_t { return bleKeypadEventQueue; }

  uint8_t NEXT_PAD;
  uint8_t PREV_PAD;
  uint8_t SELECT_PAD;
  uint8_t DNEXT_PAD;
  uint8_t DPREV_PAD;
  uint8_t HOME_PAD;

  void getEventTask(void *param) {
    EventMgr::Event event;
    // uint32_t        ioNum;
    // uint8_t         pads;

    while (true) {

      event.kind = EventMgr::EventKind::NONE;

      // In BLE_KEYPAD mode, events are generated in the BLE GAP callback and sent to the same queue.
      xQueueReceive(bleKeypadEventQueue, &event, portMAX_DELAY);

      if (event.kind != EventMgr::EventKind::NONE) {
        xQueueSend(mainEventQueue, &event, 0);
      }
    }
  }

  auto EventMgr::setOrientation(Screen::Orientation orient) -> void {
    // Nothing to do
  }

  auto EventMgr::someEventWaiting() -> bool { return uxQueueMessagesWaiting(mainEventQueue) > 0; }

  auto EventMgr::getEvent() -> const EventMgr::Event & {
    static Event event;

    int8_t       timeOutDuration = 5;
    config.get(Config::Ident::TIMEOUT, &timeOutDuration);

    while (true) {

      if (!xQueueReceive(mainEventQueue, &event, pdMS_TO_TICKS(timeOutDuration * 60000))) {

        if (!stayOn) {
          LOG_D("Timed out on BLE event reading. Going now to Deep Sleep");

          // Will not return
          gotoDeepSleep(timeOutDuration);
        }
      }

      if (event.kind == EventMgr::EventKind::PAIRING_ON) {
        showBtState.show(true, true);
      } else if (event.kind == EventMgr::EventKind::PAIRING_OFF) {
        showBtState.show(false, true);
      } else {
        return event;
      }
    }
  }

  auto EventMgr::loop() -> void {
    LOG_D("===> Loop...");
    while (1) {
      const EventMgr::Event &event = getEvent();

      LOG_D("Got event {}", (int)event.kind);
      appController.inputEvent(event);
      ESP::show_heaps_info();
      return;
    }
  }

  auto EventMgr::setup() -> bool {

    gpio_config_t io_conf;

    io_conf.intr_type    = GPIO_INTR_POSEDGE;  // Interrupt of rising edge
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_34;   // Bit mask of the pin, use GPIO34 here
    io_conf.mode         = GPIO_MODE_INPUT;   // Set as input mode
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;  // Disable pull-up mode (not available for GPIO34)

    gpio_config(&io_conf);

    // create a queue to handle key event from task
    mainEventQueue = xQueueCreate(10, sizeof(EventMgr::Event));

    // create a queue to handle key event from BLE GAP callback
    bleKeypadEventQueue = xQueueCreate(10, sizeof(EventMgr::Event));

    if (!bleKeypad.setup(bleKeypadEventQueue)) {
      MsgViewer::show(
        MsgViewer::MsgType::ALERT, false, true, "Bluetooth Problem!",
        "Failed to initialize BLE Keypad driver. ");
      return false;
    } else {
      showBtState.setup();
    }

    TaskHandle_t xHandle = nullptr;
    xTaskCreate(getEventTask, "GetEvent", 2000, nullptr, 2 | portPRIVILEGE_BIT, &xHandle);

    Wire::enter();
    io_expander_int.get_int_state();   // This is activating interrupts...
    Wire::leave();

    return true;
  }

#endif
