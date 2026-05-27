// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#if INKPLATE_6 || INKPLATE_6_EXTENDED || INKPLATE_10 || INKPLATE_10_EXTENDED ||                    \
    (EPUB_LINUX_BUILD && !TOUCH_TRIAL)

  #define __EVENT_MGR__ 1
  #include "controllers/event_mgr.hpp"

  #include "config.hpp"
  #include "controllers/app_controller.hpp"

  #include <iostream>

  #if EPUB_INKPLATE_BUILD

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

    #if EXTENDED_CASE
      #include "press_keys.hpp"
    #else
      #include "touch_keys.hpp"
    #endif

static QueueHandle_t touchpadIsrQueue   = NULL;
static QueueHandle_t touchpadEventQueue = NULL;

static void IRAM_ATTR touchpadIsrHandler(void *arg) {
  uint32_t gpioNum = (uint32_t)arg;
  xQueueSendFromISR(touchpadIsrQueue, &gpioNum, NULL);
}

    #if EXTENDED_CASE

uint8_t NEXT_PAD;
uint8_t PREV_PAD;
uint8_t SELECT_PAD;
uint8_t DNEXT_PAD;
uint8_t DPREV_PAD;
uint8_t HOME_PAD;

void getEventTask(void *param) {
  EventMgr::Event event;
  uint32_t ioNum;
  uint8_t pads;

  while (true) {

    event.kind = EventMgr::EventKind::NONE;

    xQueueReceive(touchpadIsrQueue, &ioNum, portMAX_DELAY);

    // An event interrupt happened. Retrieve pads information.
    // t1 = esp_timer_get_time();
    if ((pads = press_keys.read_all_keys()) == 0) {
      // Not fast enough or not synch with start of key strucked. Re-activating interrupts...
      Wire::enter();
      io_expander_int.get_int_state();
      Wire::leave();
    } else {
      // Wait until there is no key
      while (press_keys.read_all_keys() != 0) taskYIELD();

      // Re-activating interrupts.
      Wire::enter();
      io_expander_int.get_int_state();
      Wire::leave();

      if (pads & SELECT_PAD)
        event.kind = EventMgr::EventKind::SELECT;
      else if (pads & NEXT_PAD)
        event.kind = EventMgr::EventKind::NEXT;
      else if (pads & PREV_PAD)
        event.kind = EventMgr::EventKind::PREV;
      else if (pads & HOME_PAD)
        event.kind = EventMgr::EventKind::DBL_SELECT;
      else if (pads & DNEXT_PAD)
        event.kind = EventMgr::EventKind::DBL_NEXT;
      else if (pads & DPREV_PAD)
        event.kind = EventMgr::EventKind::DBL_PREV;
    }

    if (event.kind != EventMgr::EventKind::NONE) {
      xQueueSend(touchpadEventQueue, &event, 0);
    }
  }
}

auto EventMgr::setOrientation(Screen::Orientation orient) -> void {
  if (orient == Screen::Orientation::LEFT) {
    NEXT_PAD   = (1 << static_cast<uint8_t>(PressKeys::Key::U4));
    PREV_PAD   = (1 << static_cast<uint8_t>(PressKeys::Key::U3));
    SELECT_PAD = (1 << static_cast<uint8_t>(PressKeys::Key::U1));
    DNEXT_PAD  = (1 << static_cast<uint8_t>(PressKeys::Key::U5));
    DPREV_PAD  = (1 << static_cast<uint8_t>(PressKeys::Key::U2));
    HOME_PAD   = (1 << static_cast<uint8_t>(PressKeys::Key::U6));
  } else if (orient == Screen::Orientation::RIGHT) {
    NEXT_PAD   = (1 << static_cast<uint8_t>(PressKeys::Key::U3));
    PREV_PAD   = (1 << static_cast<uint8_t>(PressKeys::Key::U4));
    SELECT_PAD = (1 << static_cast<uint8_t>(PressKeys::Key::U6));
    DNEXT_PAD  = (1 << static_cast<uint8_t>(PressKeys::Key::U2));
    DPREV_PAD  = (1 << static_cast<uint8_t>(PressKeys::Key::U5));
    HOME_PAD   = (1 << static_cast<uint8_t>(PressKeys::Key::U1));
  } else {
    NEXT_PAD   = (1 << static_cast<uint8_t>(PressKeys::Key::U5));
    PREV_PAD   = (1 << static_cast<uint8_t>(PressKeys::Key::U2));
    SELECT_PAD = (1 << static_cast<uint8_t>(PressKeys::Key::U6));
    DNEXT_PAD  = (1 << static_cast<uint8_t>(PressKeys::Key::U3));
    DPREV_PAD  = (1 << static_cast<uint8_t>(PressKeys::Key::U4));
    HOME_PAD   = (1 << static_cast<uint8_t>(PressKeys::Key::U1));
  }
}
    #else
uint8_t NEXT_PAD;
uint8_t PREV_PAD;
uint8_t SELECT_PAD;

void getEventTask(void *param) {
  EventMgr::Event event;
  uint32_t ioNum;
  uint8_t pads, pads2;

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

        if (pads2 & SELECT_PAD)
          event.kind = EventMgr::EventKind::DBL_SELECT;
        else if (pads2 & NEXT_PAD)
          event.kind = EventMgr::EventKind::DBL_NEXT;
        else if (pads2 & PREV_PAD)
          event.kind = EventMgr::EventKind::DBL_PREV;
      } else {

        // Simple Click on a key

        if (pads & SELECT_PAD)
          event.kind = EventMgr::EventKind::SELECT;
        else if (pads & NEXT_PAD)
          event.kind = EventMgr::EventKind::NEXT;
        else if (pads & PREV_PAD)
          event.kind = EventMgr::EventKind::PREV;
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
    #endif

auto EventMgr::someEventWaiting() -> bool { return uxQueueMessagesWaiting(touchpadEventQueue) > 0; }

auto EventMgr::getEvent() -> const EventMgr::Event & {
  static Event event;
  if (!xQueueReceive(touchpadEventQueue, &event, pdMS_TO_TICKS(15E3))) {
    event.kind = EventKind::NONE;
  }
  return event;
}

  #else

    #include "screen.hpp"

  #endif

  #if EPUB_LINUX_BUILD

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

    #define BUTTON_EVENT(button, msg)                                                              \
      static void button##_clicked(GObject *button, GParamSpec *property, gpointer data) {         \
        eventMgr.button();                                                                         \
      }

BUTTON_EVENT(left, "Left Clicked")
BUTTON_EVENT(right, "Right Clicked")
BUTTON_EVENT(up, "Up Clicked")
BUTTON_EVENT(down, "Down Clicked")
BUTTON_EVENT(select, "Select Clicked")
BUTTON_EVENT(home, "Home Clicked")

auto EventMgr::someEventWaiting() -> bool { return false; }

auto EventMgr::loop() -> void {
  gtk_main(); // never return
}

auto EventMgr::setOrientation(Screen::Orientation orient) -> void {
  // Nothing to do...
}

  #else
auto EventMgr::loop() -> void {
  LOG_D("===> Loop...");
  while (1) {
    const EventMgr::Event &event = getEvent();

    if (event.kind != EventKind::NONE) {
      LOG_D("Got event %d", (int)event.kind);
      appController.inputEvent(event);
      ESP::show_heaps_info();
      return;
    } else {
      // Nothing received in 15 seconds, put the device in Light Sleep Mode.
      // After some delay, the device will then be put in Deep Sleep Mode,
      // rebooting after the user press a key.

      if (!stayOn) { // Unless somebody wants to keep us awake...
        if (pageLocs.isControlTaskReadyToBeStopped()) pageLocs.stopControlTask();

        int8_t lightSleepDuration = 5;
        config.get(Config::Ident::TIMEOUT, &lightSleepDuration);

        LOG_I("Light Sleep for %d minutes...", lightSleepDuration);
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
      }
    }
  }
}
  #endif

auto EventMgr::setup() -> bool {
  #if EPUB_LINUX_BUILD
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

  #else

  gpio_config_t io_conf;

  io_conf.intr_type    = GPIO_INTR_POSEDGE;   // Interrupt of rising edge
  io_conf.pin_bit_mask = 1ULL << GPIO_NUM_34; // Bit mask of the pin, use GPIO34 here
  io_conf.mode         = GPIO_MODE_INPUT;     // Set as input mode
  io_conf.pull_up_en   = GPIO_PULLUP_DISABLE; // Disable pull-up mode (not available for GPIO34)

  gpio_config(&io_conf);

  touchpadIsrQueue   = xQueueCreate( // create a queue to handle gpio event from isr
      10, sizeof(uint32_t));
  touchpadEventQueue = xQueueCreate( // create a queue to handle key event from task
      10, sizeof(EventMgr::Event));

  TaskHandle_t xHandle = nullptr;
  xTaskCreate(getEventTask, "GetEvent", 2000, nullptr, 2 | portPRIVILEGE_BIT, &xHandle);

  gpio_install_isr_service(0); // install gpio isr service

  gpio_isr_handler_add( // hook isr handler for specific gpio pin
      GPIO_NUM_34, touchpadIsrHandler, (void *)GPIO_NUM_34);

  Wire::enter();
  io_expander_int.get_int_state(); // This is activating interrupts...
  Wire::leave();
  #endif

  return true;
}

#endif
