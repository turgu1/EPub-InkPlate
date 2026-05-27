// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.
//
//
// Touch Screen calibration algorithm from: https://www.embedded.com/how-to-calibrate-touch-screens
//

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL

  #define __EVENT_MGR__ 1
  #include "controllers/event_mgr.hpp"

  #include <cmath>
  #include <iostream>

  #include "config.hpp"
  #include "controllers/app_controller.hpp"
  #include "helpers/goto_deep_sleep.hpp"
  #include "logging.hpp"
  #include "models/page_locs.hpp"
  #include "viewers/page.hpp"

const char *EventMgr::eventStr[9] = {"NONE",         "TAP",         "HOLD",
                                     "SWIPE_LEFT",   "SWIPE_RIGHT", "PINCH_ENLARGE",
                                     "PINCH_REDUCE", "RELEASE",     "WAKEUP_BUTTON"};

  #define U16(v) static_cast<uint16_t>(v)

  #if EPUB_INKPLATE_BUILD

    #include "driver/gpio.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/queue.h"
    #include "freertos/task.h"

    #include "inkplate_platform.hpp"
    #include "viewers/msg_viewer.hpp"
    #include "viewers/screen_saver.hpp"
    #include "wire.hpp"

    #if INKPLATE_6FLICK
      #include "touch_screen_cypress.hpp"
    #else
      #include "touch_screen_elan.hpp"
    #endif

    #include "logging.hpp"

static QueueHandle_t touchscreenIsrQueue   = NULL;
static QueueHandle_t touchscreenEventQueue = NULL;

static void IRAM_ATTR touchscreenIsrHandler(void *arg) {
  uint32_t gpioNum = (uint32_t)arg;
  xQueueSendFromISR(touchscreenIsrQueue, &gpioNum, NULL);
}

    #define DISTANCE (sqrt(pow(xEnd - xStart, 2) + pow(yEnd - yStart, 2)))
    #define DISTANCE2 (sqrt(pow(x[1] - x[0], 2) + pow(y[1] - y[0], 2)))

void getEventTask(void *param) {
  static constexpr char const *TAG = "GetEventTask";

  const uint32_t maxDelayMs        = 4E3; // 10 seconds
  const uint16_t distanceThreshold = 30;  // treshold distance in pixels

  enum class State : uint8_t { NONE, WAIT_NEXT, HOLDING, SWIPING, PINCHING };

  EventMgr::Event event;

  TouchScreen::TouchPositions x, y;

  uint16_t xStart = 0, yStart = 0, xEnd = 0, yEnd = 0, lastDist = 0;

  uint32_t ioNum;
  uint8_t count;

  State state  = State::NONE;
  bool timeout = false;

  // while (true) {
  //   xQueueReceive(touchscreenIsrQueue, &ioNum, portMAX_DELAY);
  //   count = touch_screen.getPosition(x, y);
  //   LOG_I("Got: %d, [%d, %d] [%d, %d]", count, x[0], y[0], x[1], y[1]);
  // }

  while (true) {
    event = {.kind = EventMgr::EventKind::NONE, .x = 0, .y = 0, .dist = 0};

    LOG_D("State: %" PRIu8, uint8_t(state));

    switch (state) {
    case State::NONE:
      xQueueReceive(touchscreenIsrQueue, &ioNum, portMAX_DELAY);
      count = touch_screen.get_position(x, y);
      if (timeout) {
        timeout = false;
      } else if (count == 1) {
        state = State::WAIT_NEXT;
        eventMgr.setPosition(x[0], y[0]);
        eventMgr.toUserCoord(x[0], y[0]);
        xStart = x[0];
        yStart = y[0];
      } else if (count == 2) {
        eventMgr.toUserCoord(x[0], y[0]);
        eventMgr.toUserCoord(x[1], y[1]);
        event.dist = lastDist = DISTANCE2;
        state                 = State::PINCHING;
      }
      // else if (count == 0) {
      //   event.kind = EventMgr::EventKind::WAKEUP_BUTTON;
      // }
      break;

    case State::WAIT_NEXT:
      if (xQueueReceive(touchscreenIsrQueue, &ioNum, 500 / portTICK_PERIOD_MS)) {
        count = touch_screen.get_position(x, y);
        if (count == 0) {
          state      = State::NONE;
          event.kind = EventMgr::EventKind::TAP;
          event.x    = xStart;
          event.y    = yStart;
        } else if (count == 1) {
          eventMgr.setPosition(x[0], y[0]);
          eventMgr.toUserCoord(x[0], y[0]);
          xEnd = x[0];
          yEnd = y[0];
          if (DISTANCE > distanceThreshold) {
            state = State::SWIPING;
          }
        } else if (count == 2) {
          eventMgr.toUserCoord(x[0], y[0]);
          eventMgr.toUserCoord(x[1], y[1]);
          event.dist = lastDist = DISTANCE2;
          state                 = State::PINCHING;
        }
      } else {
        state      = State::HOLDING;
        event.kind = EventMgr::EventKind::HOLD;
        event.x    = xStart;
        event.y    = yStart;
      }
      break;

    case State::HOLDING:
      ESP::delay_microseconds(1000);
      if (xQueueReceive(touchscreenIsrQueue, &ioNum, maxDelayMs / portTICK_PERIOD_MS)) {
        count = touch_screen.get_position(x, y);
        if (count == 0) {
          event.kind = EventMgr::EventKind::RELEASE;
          state      = State::NONE;
        }
      } else { // timeout
        event.kind = EventMgr::EventKind::RELEASE;
        state      = State::NONE;
        timeout    = true;
      }
      break;

    case State::SWIPING:
      if (xQueueReceive(touchscreenIsrQueue, &ioNum, maxDelayMs / portTICK_PERIOD_MS)) {
        count = touch_screen.get_position(x, y);
        if (count == 0) {
          event.kind =
              (xStart < xEnd) ? EventMgr::EventKind::SWIPE_RIGHT : EventMgr::EventKind::SWIPE_LEFT;
          state = State::NONE;
        }
        if (count == 1) {
          eventMgr.setPosition(x[0], y[0]);
          eventMgr.toUserCoord(x[0], y[0]);
          xEnd = x[0];
          yEnd = y[0];
        } else if (count == 2) {
          eventMgr.toUserCoord(x[0], y[0]);
          eventMgr.toUserCoord(x[1], y[1]);
          event.dist = lastDist = DISTANCE2;
          state                 = State::PINCHING;
        }
      } else { // timeout
        event.kind =
            (xStart < xEnd) ? EventMgr::EventKind::SWIPE_RIGHT : EventMgr::EventKind::SWIPE_LEFT;
        state   = State::NONE;
        timeout = true;
      }
      break;

    case State::PINCHING:
      if (xQueueReceive(touchscreenIsrQueue, &ioNum, maxDelayMs / portTICK_PERIOD_MS)) {
        count = touch_screen.get_position(x, y);
        if (count == 0) {
          event.kind = EventMgr::EventKind::RELEASE;
          state      = State::NONE;
        } else if (count == 2) {
          eventMgr.toUserCoord(x[0], y[0]);
          eventMgr.toUserCoord(x[1], y[1]);
          uint16_t thisDist    = DISTANCE2;
          uint16_t newDistDiff = abs(lastDist - thisDist);
          LOG_D("Distance diffs: %u %u", lastDist, newDistDiff);
          if (newDistDiff != 0) {
            event.kind = (lastDist < thisDist) ? EventMgr::EventKind::PINCH_ENLARGE
                                               : EventMgr::EventKind::PINCH_REDUCE;
            event.dist = newDistDiff;
            lastDist   = thisDist;
          }
        }
      } else {
        event.kind = EventMgr::EventKind::RELEASE;
        state      = State::NONE;
        timeout    = true;
      }
      break;
    }

    if (event.kind != EventMgr::EventKind::NONE) {
      LOG_D("Input Event %s [%u, %u] (%u)...", EventMgr::eventStr[int(event.kind)], event.x,
            event.y, event.dist);
      xQueueSend(touchscreenEventQueue, &event, 0);
      taskYIELD();
    }
  }
}

auto EventMgr::setOrientation(Screen::Orientation orient) -> void {}

auto EventMgr::someEventWaiting() -> bool {
  return uxQueueMessagesWaiting(touchscreenEventQueue) > 0;
}

const EventMgr::Event &EventMgr::getEvent() {
  static Event event;
  if (!xQueueReceive(touchscreenEventQueue, &event, pdMS_TO_TICKS(15E3))) {
    event.kind = EventKind::NONE;
  }
  return event;
}

auto EventMgr::showCalibration() -> void {
  auto page = Page::Make(appFonts);

  switch (screen.getOrientation()) {
  case Screen::Orientation::BOTTOM:
    LOG_D("Bottom...");
    calibPoint = {.x = {U16(Screen::getWidth() - 100), U16(Screen::getWidth() / 2), 100},
                  .y = {100, U16(Screen::getHeight() - 100), U16(Screen::getHeight() / 2)}};
    break;

  case Screen::Orientation::TOP:
    LOG_D("Top...");
    calibPoint = {.x = {100, U16(Screen::getWidth() / 2), U16(Screen::getWidth() - 100)},
                  .y = {U16(Screen::getHeight() - 100), 100, U16(Screen::getHeight() / 2)}};
    break;

  case Screen::Orientation::LEFT:
    LOG_D("Left...");
    calibPoint = {.x = {U16(Screen::getWidth() - 100), 100, U16(Screen::getWidth() / 2)},
                  .y = {U16(Screen::getHeight() - 100), U16(Screen::getHeight() / 2), 100}};
    break;

  case Screen::Orientation::RIGHT:
    LOG_D("Right...");
    calibPoint = {.x = {100, U16(Screen::getWidth() - 100), U16(Screen::getWidth() / 2)},
                  .y = {100, U16(Screen::getHeight() / 2), U16(Screen::getHeight() - 100)}};
    break;
  }

  auto crossHair = [&page](uint16_t x, uint16_t y) {
    page->putHighlight(Dim(41, 3), Pos(x - 20, y - 1));
    page->putHighlight(Dim(43, 5), Pos(x - 21, y - 2));
    page->putHighlight(Dim(45, 7), Pos(x - 22, y - 3));

    page->putHighlight(Dim(3, 41), Pos(x - 1, y - 20));
    page->putHighlight(Dim(5, 43), Pos(x - 2, y - 21));
    page->putHighlight(Dim(7, 45), Pos(x - 3, y - 22));
  };

  for (int i = 0; i < 3; ++i) crossHair(calibPoint.x[i], calibPoint.y[i]);

  Page::Format fmt = {
      .fontSize     = 11,
      .marginLeft   = 5,
      .marginRight  = 5,
      .screenLeft   = 20,
      .screenRight  = 20,
      .screenTop    = 0,
      .screenBottom = 0,
      .align        = CSS::Align::CENTER,
  };

  page->putStrAt("Touch Sensor Calibration",
                 Pos(Screen::getWidth() >> 1, (Screen::getHeight() >> 1) - 150), fmt);
  page->putStrAt("Please TAP once on each crosshair to calibrate.",
                 Pos(Screen::getWidth() >> 1, (Screen::getHeight() >> 1) - 100), fmt);

  calibCount = 0;
  page->paint();
}

auto EventMgr::toUserCoord(uint16_t &x, uint16_t &y) -> void {
  uint16_t temp;
  Screen::Orientation orientation = screen.getOrientation();

  if (a == 0) {
    LOG_I("Calibration required!");
    switch (orientation) {
    case Screen::Orientation::BOTTOM:
      LOG_D("Bottom...");
      temp = y;
      y    = ((uint32_t)(Screen::getHeight() - 1) * x) / touch_screen.get_x_resolution();
      x    = Screen::getWidth() -
          ((uint32_t)(Screen::getWidth() - 1) * temp) / touch_screen.get_y_resolution();
      break;

    case Screen::Orientation::TOP:
      LOG_D("Top...");
      temp = y;
      y    = Screen::getHeight() -
          ((uint32_t)(Screen::getHeight() - 1) * x) / touch_screen.get_x_resolution();
      x = ((uint32_t)(Screen::getWidth() - 1) * temp) / touch_screen.get_y_resolution();
      break;

    case Screen::Orientation::LEFT:
      LOG_D("Left...");
      x = Screen::getWidth() -
          ((uint32_t)(Screen::getWidth() - 1) * x) / touch_screen.get_x_resolution();
      y = Screen::getHeight() -
          ((uint32_t)(Screen::getHeight() - 1) * y) / touch_screen.get_y_resolution();
      break;

    case Screen::Orientation::RIGHT:
      LOG_D("Right...");
      x = ((uint32_t)(Screen::getWidth() - 1) * x) / touch_screen.get_x_resolution();
      y = ((uint32_t)(Screen::getHeight() - 1) * y) / touch_screen.get_y_resolution();
      break;
    }
  } else {
    LOG_D("Coordinates to transform: [%u, %u]", x, y);

    uint16_t xd = (a * x + b * y + c) / divider;
    uint16_t yd = (d * x + e * y + f) / divider;

    switch (orientation) {
    case Screen::Orientation::BOTTOM:
      LOG_D("Bottom...");
      temp = y;
      y    = xd;
      ;
      x = Screen::getWidth() - yd;
      break;

    case Screen::Orientation::TOP:
      LOG_D("Top...");
      temp = y;
      y    = Screen::getHeight() - xd;
      x    = yd;
      break;

    case Screen::Orientation::LEFT:
      LOG_D("Left...");
      x = Screen::getWidth() - xd;
      y = Screen::getHeight() - yd;
      break;

    case Screen::Orientation::RIGHT:
      LOG_D("Right...");
      x = xd;
      y = yd;
      break;
    }

    LOG_D("Result of transform: [%u, %u]", x, y);
  }
}

auto EventMgr::calibrationEvent(const Event &event) -> bool {
  switch (event.kind) {
  case EventKind::TAP:
    if (calibCount < 3) {
      getPosition(touchPoint.x[calibCount], touchPoint.y[calibCount]);
      ++calibCount;
    }
    if (calibCount >= 3) {
      uint16_t temp;
    #define SWAP(i0, i1)                                                                           \
      {                                                                                            \
        temp             = touchPoint.x[i0];                                                       \
        touchPoint.x[i0] = touchPoint.x[i1];                                                       \
        touchPoint.x[i1] = temp;                                                                   \
        temp             = touchPoint.y[i0];                                                       \
        touchPoint.y[i0] = touchPoint.y[i1];                                                       \
        touchPoint.y[i1] = temp;                                                                   \
      }
      if (touchPoint.y[0] > touchPoint.y[2]) SWAP(0, 2);
      if (touchPoint.y[0] > touchPoint.y[1]) SWAP(0, 1);
      if (touchPoint.y[1] > touchPoint.y[2]) SWAP(1, 2);

    #undef SWAP

    #define X0 touchPoint.x[0]
    #define X1 touchPoint.x[1]
    #define X2 touchPoint.x[2]
    #define Y0 touchPoint.y[0]
    #define Y1 touchPoint.y[1]
    #define Y2 touchPoint.y[2]

      calibPoint = {.x = {100, U16(e_ink.get_height() - 100), U16(e_ink.get_height() / 2)},
                    .y = {100, U16(e_ink.get_width() / 2), U16(e_ink.get_width() - 100)}};

    #define XD0 calibPoint.x[0]
    #define XD1 calibPoint.x[1]
    #define XD2 calibPoint.x[2]
    #define YD0 calibPoint.y[0]
    #define YD1 calibPoint.y[1]
    #define YD2 calibPoint.y[2]

      LOG_D("Touch points: [%u, %u] [%u, %u] [%u, %u]", X0, Y0, X1, Y1, X2, Y2);
      LOG_D("Calib points: [%u, %u] [%u, %u] [%u, %u]", XD0, YD0, XD1, YD1, XD2, YD2);

      divider = ((X0 - X2) * (Y1 - Y2)) - ((X1 - X2) * (Y0 - Y2));

      a = (((XD0 - XD2) * (Y1 - Y2)) - ((XD1 - XD2) * (Y0 - Y2)));
      b = (((X0 - X2) * (XD1 - XD2)) - ((XD0 - XD2) * (X1 - X2)));
      c = ((Y0 * ((X2 * XD1) - (X1 * XD2))) + (Y1 * ((X0 * XD2) - (X2 * XD0))) +
           (Y2 * ((X1 * XD0) - (X0 * XD1))));
      d = (((YD0 - YD2) * (Y1 - Y2)) - ((YD1 - YD2) * (Y0 - Y2)));
      e = (((X0 - X2) * (YD1 - YD2)) - ((YD0 - YD2) * (X1 - X2)));
      f = ((Y0 * ((X2 * YD1) - (X1 * YD2))) + (Y1 * ((X0 * YD2) - (X2 * YD0))) +
           (Y2 * ((X1 * YD0) - (X0 * YD1))));

      LOG_D("Coefficients: a:%lld b:%lld c:%lld d:%lld e:%lld f:%lld divider:%lld", a, b, c, d, e,
            f, divider);

    #undef X0
    #undef X1
    #undef X2
    #undef Y0
    #undef Y1
    #undef Y2

    #undef XD0
    #undef XD1
    #undef XD2
    #undef YD0
    #undef YD1
    #undef YD2

      config.put(Config::Ident::CALIB_A, a);
      config.put(Config::Ident::CALIB_B, b);
      config.put(Config::Ident::CALIB_C, c);
      config.put(Config::Ident::CALIB_D, d);
      config.put(Config::Ident::CALIB_E, e);
      config.put(Config::Ident::CALIB_F, f);
      config.put(Config::Ident::CALIB_DIVIDER, divider);
      config.save();

      return true;
    }
    break;

  default:
    break;
  }

  return false;
}

auto EventMgr::retrieveCalibrationValues() -> void {
  config.get(Config::Ident::CALIB_A, &a);
  config.get(Config::Ident::CALIB_B, &b);
  config.get(Config::Ident::CALIB_C, &c);
  config.get(Config::Ident::CALIB_D, &d);
  config.get(Config::Ident::CALIB_E, &e);
  config.get(Config::Ident::CALIB_F, &f);
  config.get(Config::Ident::CALIB_DIVIDER, &divider);
}
  #endif

  #if EPUB_LINUX_BUILD

    #include <gtk/gtk.h>

    #include "screen.hpp"

static auto mouseEventCallback(GtkWidget *event_box, GdkEvent *gdk_event, gpointer data)
    -> gboolean {
    #define DISTANCE (sqrt(pow(xEnd - xStart, 2) + pow(yEnd - yStart, 2)))
    #define DISTANCE2 ((x - xStart) < 0 ? 0 : (x - xStart))

  static constexpr char const *TAG = "MouseEventCallback";

  const uint32_t maxDelayMs        = 4E3; // 4 seconds
  const uint16_t distanceThreshold = 30;  // treshold distance in pixels

  enum class State : uint8_t { NONE, WAIT_NEXT, HOLDING, SWIPING, PINCHING };

  static uint16_t xStart = 0, yStart = 0, xEnd = 0, yEnd = 0, lastDist = 0;

  static State state  = State::NONE;
  static bool timeout = false;

  EventMgr::Event event = {.kind = EventMgr::EventKind::NONE, .x = 0, .y = 0, .dist = 0};

  uint8_t count;
  uint16_t x = ((GdkEventButton *)gdk_event)->x;
  uint16_t y = ((GdkEventButton *)gdk_event)->y;

  if (gdk_event->type == GDK_BUTTON_RELEASE) {
    count = 0;
  } else if ((state != State::NONE) || (gdk_event->type != GDK_ENTER_NOTIFY)) {
    count = ((GdkEventButton *)gdk_event)->button == 3 ? 2 : 1;
  } else {
    count = 0;
  }

  LOG_D("State: %u", std::static_cast<uint8_t>(state));

  switch (state) {
  case State::NONE:
    if (timeout) {
      timeout = false;
    } else if (((GdkEventButton *)gdk_event)->state & GDK_CONTROL_MASK) {
      state      = State::HOLDING;
      event.kind = EventMgr::EventKind::HOLD;
      event.x    = x;
      event.y    = y;
    } else if (count == 1) {
      state = State::WAIT_NEXT;
      eventMgr.setPosition(x, y);
      xStart = x;
      yStart = y;
    } else if (count == 2) {
      xStart     = x;
      yStart     = y;
      event.dist = lastDist = DISTANCE2;
      state                 = State::PINCHING;
    }
    break;

  case State::WAIT_NEXT:
    if (count == 0) {
      state      = State::NONE;
      event.kind = EventMgr::EventKind::TAP;
      event.x    = xStart;
      event.y    = yStart;
    } else if (count == 1) {
      eventMgr.setPosition(x, y);
      xEnd = x;
      yEnd = y;
      if (DISTANCE > distanceThreshold) {
        state = State::SWIPING;
      }
    } else if (count == 2) {
      event.dist = lastDist = DISTANCE2;
      state                 = State::PINCHING;
    }
    break;

  case State::HOLDING:
    if (count == 0) {
      event.kind = EventMgr::EventKind::RELEASE;
      state      = State::NONE;
    }
    break;

  case State::SWIPING:
    if (count == 0) {
      event.kind =
          (xStart < xEnd) ? EventMgr::EventKind::SWIPE_RIGHT : EventMgr::EventKind::SWIPE_LEFT;
      state = State::NONE;
    }
    if (count == 1) {
      eventMgr.setPosition(x, y);
      xEnd = x;
      yEnd = y;
    } else if (count == 2) {
      event.dist = lastDist = DISTANCE2;
      state                 = State::PINCHING;
    }
    break;

  case State::PINCHING:
    if (count == 0) {
      event.kind = EventMgr::EventKind::RELEASE;
      state      = State::NONE;
    } else if (count == 2) {
      uint16_t thisDist    = DISTANCE2;
      uint16_t newDistDiff = abs(lastDist - thisDist);
      LOG_D("Distance diffs: %u %u", lastDist, newDistDiff);
      if (newDistDiff != 0) {
        event.kind = (lastDist < thisDist) ? EventMgr::EventKind::PINCH_ENLARGE
                                           : EventMgr::EventKind::PINCH_REDUCE;
        event.dist = newDistDiff;
        lastDist   = thisDist;
      }
    }
    break;
  }

  if (event.kind != EventMgr::EventKind::NONE) {
    LOG_D("Input Event %s [%u, %u] (%u)...", EventMgr::eventStr[int(event.kind)], event.x, event.y,
          event.dist);
    appController.inputEvent(event);
    appController.launch();
    return true;
  }

  return false;
}

auto EventMgr::loop() -> void {
  gtk_main(); // never return
}

auto EventMgr::setOrientation(Screen::Orientation orient) -> void {
  // Nothing to do...
}

  #else
auto EventMgr::loop() -> void {
  while (1) {
    const EventMgr::Event &event = getEvent();

    if (event.kind != EventKind::NONE) {
      LOG_D("Got Event %d", (int)event.kind);
      appController.inputEvent(event);
      ESP::show_heaps_info();
      return;
    } else {
      // Nothing received in 15 seconds, put the device in Light Sleep Mode.
      // After some delay, the device will then be put in Deep Sleep Mode,
      // rebooting after the user press a key.

      if (!stayOn) { // Unless somebody wants to keep us awake...
        if (pageLocs.isControlTaskReadyToBeStopped()) pageLocs.stopControlTask();

        int8_t lightSleepDuration = 2;
        config.get(Config::Ident::TIMEOUT, &lightSleepDuration);

        LOG_D("Light Sleep for %d minutes...", lightSleepDuration);
        ESP::delay(500);

        if (inkplate_platform.light_sleep(lightSleepDuration, TouchScreen::INTERRUPT_PIN, 0)) {
          LOG_I("Timed out on Light Sleep. Going now to Deep Sleep");

          gotoDeepSleep(lightSleepDuration);
        }
      }
    }
  }
}
  #endif

auto EventMgr::setup() -> bool {
  #if EPUB_LINUX_BUILD

  g_signal_connect(G_OBJECT(screen.pictureBox), "event", G_CALLBACK(mouseEventCallback),
                   screen.getPicture());
  #else

  retrieveCalibrationValues();

  touchscreenIsrQueue   = xQueueCreate( // create a queue to handle gpio event from isr
      20, sizeof(uint32_t));
  touchscreenEventQueue = xQueueCreate( // create a queue to handle event from task
      20, sizeof(EventMgr::Event));

  touch_screen.set_app_isr_handler(touchscreenIsrHandler);

  TaskHandle_t taskHandle = NULL;
  xTaskCreate(getEventTask, "GetEvent", 2000, nullptr, 10, &taskHandle);

  #endif

  return true;
}

#endif
