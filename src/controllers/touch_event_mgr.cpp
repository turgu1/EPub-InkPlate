// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#if INKPLATE_6PLUS || TOUCH_TRIAL

#define __EVENT_MGR__ 1
#include "controllers/event_mgr.hpp"

#include <iostream>

#include "controllers/app_controller.hpp"
#include "viewers/page.hpp"
#include "models/config.hpp"

const char * EventMgr::event_str[8] = { "NONE",        "TAP",           "HOLD",         "SWIPE_LEFT", 
                                        "SWIPE_RIGHT", "PINCH_ENLARGE", "PINCH_REDUCE", "RELEASE"    };


#if EPUB_INKPLATE_BUILD

  #include <cmath>

  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
  #include "driver/gpio.h"

  #include "wire.hpp"
  #include "inkplate_platform.hpp"
  #include "viewers/msg_viewer.hpp"

  #include "touch_screen.hpp"
  #include "logging.hpp"

  static xQueueHandle touchscreen_isr_queue   = NULL;
  static xQueueHandle touchscreen_event_queue = NULL;

  static void IRAM_ATTR 
  touchscreen_isr_handler(void * arg)
  {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(touchscreen_isr_queue, &gpio_num, NULL);
  }

  #define DISTANCE  (sqrt(pow(x_end - x_start, 2) + pow(y_end - y_start, 2)))
  #define DISTANCE2 (sqrt(pow(x[1] - x[0], 2) + pow(y[1] - y[0], 2)))

  void
  get_event_task(void * param)
  {
    static constexpr char const * TAG = "GetEventTask";

    const uint32_t MAX_DELAY_MS  =     4E3; // 10 seconds
    const uint16_t DIST_TRESHOLD =      30; // treshold distance in pixels

    enum class State : uint8_t { NONE, WAIT_NEXT, HOLDING, SWIPING, PINCHING };

    EventMgr::Event event;

    TouchScreen::TouchPositions x, y;

    uint16_t 
      x_start    = 0, 
      y_start    = 0, 
      x_end      = 0, 
      y_end      = 0,
      last_dist  = 0;

    uint32_t io_num;
    uint8_t  count;
    
    State    state   = State::NONE;
    bool     timeout = false;

    // while (true) {
    //   xQueueReceive(touchscreen_isr_queue, &io_num, portMAX_DELAY);
    //   count = touch_screen.get_position(x, y);
    //   LOG_I("Got: %d, [%d, %d] [%d, %d]", count, x[0], y[0], x[1], y[1]);
    // }

    while (true) {
      event = {
        .kind = EventMgr::EventKind::NONE,
        .x    = 0,
        .y    = 0,
        .dist = 0
      };

      LOG_D("State: %u", (uint8_t)state);
      switch (state) {
        case State::NONE:
          xQueueReceive(touchscreen_isr_queue, &io_num, portMAX_DELAY);
          count = touch_screen.get_position(x, y);
          if (timeout) {
            timeout = false;
          }
          else if (count == 1) {
            state = State::WAIT_NEXT;
            screen.to_user_coord(x[0], y[0]);
            event_mgr.set_position(x[0], y[0]);
            x_start = x[0];
            y_start = y[0];
          }
          else if (count == 2) {
            screen.to_user_coord(x[0], y[0]);
            screen.to_user_coord(x[1], y[1]);
            event.dist = last_dist = DISTANCE2;
            state      = State::PINCHING;
          }
          break; 

        case State::WAIT_NEXT:
          if (xQueueReceive(touchscreen_isr_queue, &io_num, 500 / portTICK_PERIOD_MS)) {
            count = touch_screen.get_position(x, y);
            if (count == 0) {
              state      = State::NONE;
              event.kind = EventMgr::EventKind::TAP;
              event.x    = x_start;
              event.y    = y_start;
            }
            else if (count == 1) {
              screen.to_user_coord(x[0], y[0]);
              event_mgr.set_position(x[0], y[0]);
              x_end = x[0];
              y_end = y[0];
              if (DISTANCE > DIST_TRESHOLD) {
                state = State::SWIPING;
              }
            }
            else if (count == 2) {
              screen.to_user_coord(x[0], y[0]);
              screen.to_user_coord(x[1], y[1]);
              event.dist = last_dist = DISTANCE2;
              state      = State::PINCHING;
            }
          }
          else {
            state      = State::HOLDING;
            event.kind = EventMgr::EventKind::HOLD;
            event.x    = x_start;
            event.y    = y_start;
          }
          break;

        case State::HOLDING:
          ESP::delay_microseconds(1000);
          if (xQueueReceive(touchscreen_isr_queue, &io_num, MAX_DELAY_MS / portTICK_PERIOD_MS)) {
            count = touch_screen.get_position(x, y);
            if (count == 0) {
              event.kind = EventMgr::EventKind::RELEASE;
              state      = State::NONE;
            }
          }
          else { // timeout
            event.kind   = EventMgr::EventKind::RELEASE;
            state        = State::NONE;
            timeout      = true;
          }
          break;

        case State::SWIPING:
          if (xQueueReceive(touchscreen_isr_queue, &io_num, MAX_DELAY_MS / portTICK_PERIOD_MS)) {
            count = touch_screen.get_position(x, y);
            if (count == 0) {
              event.kind = (x_start < x_end) ? EventMgr::EventKind::SWIPE_RIGHT : 
                                               EventMgr::EventKind::SWIPE_LEFT; 
              state = State::NONE;
            }
            if (count == 1) {
              screen.to_user_coord(x[0], y[0]);
              event_mgr.set_position(x[0], y[0]);
              x_end = x[0];
              y_end = y[0];
            }
            else if (count == 2) {
              screen.to_user_coord(x[0], y[0]);
              screen.to_user_coord(x[1], y[1]);
              event.dist = last_dist = DISTANCE2;
              state      = State::PINCHING;
            }
          }
          else { // timeout
            event.kind   = (x_start < x_end) ? EventMgr::EventKind::SWIPE_RIGHT : 
                                               EventMgr::EventKind::SWIPE_LEFT; 
            state   = State::NONE;
            timeout = true;
          }
          break;

        case State::PINCHING:
          if (xQueueReceive(touchscreen_isr_queue, &io_num, MAX_DELAY_MS / portTICK_PERIOD_MS)) {
            count = touch_screen.get_position(x, y);
            if (count == 0) {
              event.kind = EventMgr::EventKind::RELEASE;
              state      = State::NONE;
            }
            else if (count == 2) {
              screen.to_user_coord(x[0], y[0]);
              screen.to_user_coord(x[1], y[1]);
              uint16_t this_dist = DISTANCE2;
              uint16_t new_dist_diff = abs(last_dist - this_dist);
              LOG_D("Distance diffs: %u %u", last_dist, new_dist_diff);
              if (new_dist_diff != 0) {
                event.kind = (last_dist < this_dist) ? 
                                EventMgr::EventKind::PINCH_ENLARGE : 
                                EventMgr::EventKind::PINCH_REDUCE;
                event.dist = new_dist_diff;
                last_dist  = this_dist;
              }
            }
          }
          else {
            event.kind = EventMgr::EventKind::RELEASE;
            state      = State::NONE;
            timeout    = true;
          }
          break;
      }

      if (event.kind != EventMgr::EventKind::NONE) {
        LOG_D("Input Event %s [%u, %u] (%u)...", 
              EventMgr::event_str[int(event.kind)], 
              event.x, event.y,
              event.dist);
        xQueueSend(touchscreen_event_queue, &event, 0);
        taskYIELD();
      }
    }
  }

  void
  EventMgr::set_orientation(Screen::Orientation orient)
  {
  }

  const EventMgr::Event & 
  EventMgr::get_event() 
  {
    static Event event;
    if (!xQueueReceive(touchscreen_event_queue, &event, pdMS_TO_TICKS(15E3))) {
      event.kind = EventKind::NONE;
    }
    return event;
  }

  void 
  EventMgr::show_calibration()
  {
    auto cross_hair = [](uint16_t x, uint16_t y) {
      page.put_highlight(Dim(41, 3), Pos(x - 20, y - 1));
      page.put_highlight(Dim(43, 5), Pos(x - 21, y - 2));
      page.put_highlight(Dim(45, 7), Pos(x - 22, y - 3));

      page.put_highlight(Dim(3, 41), Pos(x - 1, y - 20));
      page.put_highlight(Dim(5, 43), Pos(x - 2, y - 21));
      page.put_highlight(Dim(7, 45), Pos(x - 3, y - 22));
    };

    cross_hair(100, 100);
    cross_hair(screen.WIDTH - 100, screen.HEIGHT - 100);

    calib_count = 0;
    page.paint();
  }

  bool EventMgr::calibration_event(const Event & event)
  {
    switch (event.kind) {
      case EventKind::TAP:
        if (calib_count < 2) {
          get_position(calib_data.x[calib_count], calib_data.y[calib_count]);
          calib_count++;
        }
        return calib_count >= 2;
        break;

      default:
        break;
    }

    return false;
  }

#endif

#if EPUB_LINUX_BUILD

  #include <gtk/gtk.h>

  #include "screen.hpp"

  void 
  EventMgr::low_input_event(EventMgr::LowInputEvent low_event, 
                            uint16_t x, 
                            uint16_t y, 
                            bool hold)
  {
    Event event.kind = Event::NONE;

    if (low_event == LowInputEvent::PRESS1) {
      x_pos = x_start = x;
      y_pos = y_start = y;
      if (hold) {
        event = Event::HOLD;
        state = State::HOLDING;
        // LOG_D("Holding...");
      }
      else {
        state = State::TAPING;
        // LOG_D("Taping...");
      }
    }
    else if (low_event == LowInputEvent::PRESS2) {
      x_pos = x_start = x;
      y_pos = y_start = y;
      state = State::PINCHING;
      // LOG_D("Pinching...")
    }
    else if (low_event == LowInputEvent::MOVE) {
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
          event = (x < x_pos) ? Event::PINCH_REDUCE : Event::PINCH_ENLARGE;
          x_pos = x;
          y_pos = y;
        }
      }
    }
    else if (low_event == LowInputEvent::RELEASE) {
      x_pos = x;
      y_pos = y;
      if (state == State::TAPING) {
        event = Event::TAP;
      }
      else if (state == State::SWIPING) {
        event = (x < x_start) ? Event::SWIPE_LEFT : Event::SWIPE_RIGHT;
      }
      else { // PINCHING or HOLDING
        event = Event::RELEASE;
      }
      state = State::NONE;
      // LOG_D("None...");
    }
    if (event != Event::NONE) {
      LOG_D("Input Event %s [%d, %d] [%d, %d]...", 
            event_str[int(event)], x_start, y_start, x_pos, y_pos);
      app_controller.input_event(event);
      app_controller.launch();
    }
  }

  static gboolean
  mouse_event_callback(GtkWidget * event_box,
                       GdkEvent  * gdk_event,
                       gpointer    data) 
  {
    EventMgr::LowInputEvent low_event = EventMgr::LowInputEvent::NONE;
    uint16_t x, y;

    switch (gdk_event->type) {
      case GDK_BUTTON_RELEASE:
        x = ((GdkEventButton *) gdk_event)->x;
        y = ((GdkEventButton *) gdk_event)->y;
        low_event = EventMgr::LowInputEvent::RELEASE;
        break;
      case GDK_BUTTON_PRESS:
        x = ((GdkEventButton *) gdk_event)->x;
        y = ((GdkEventButton *) gdk_event)->y;
        if (((GdkEventButton *) gdk_event)->button == 1) {
          low_event = EventMgr::LowInputEvent::PRESS1;
        }
        else if (((GdkEventButton *) gdk_event)->button == 3) { // simulate pinch
          low_event = EventMgr::LowInputEvent::PRESS2;
        }
        break;
      case GDK_MOTION_NOTIFY:
        x = ((GdkEventMotion *) gdk_event)->x;
        y = ((GdkEventMotion *) gdk_event)->y;
        low_event = EventMgr::LowInputEvent::MOVE;
        break;
      default:
        break;
    }

    if (low_event != EventMgr::LowInputEvent::NONE) {
      event_mgr.low_input_event(low_event, x, y, 
                                ((GdkEventButton *) gdk_event)->state & GDK_CONTROL_MASK);
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
      const EventMgr::Event & event = get_event();

      if (event.kind != EventKind::NONE) {
        LOG_D("Got Event %d", (int)event.kind);
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

          if (inkplate_platform.light_sleep(light_sleep_duration, TouchScreen::INTERRUPT_PIN, 0)) {

            app_controller.going_to_deep_sleep();
            
            LOG_D("Timed out on Light Sleep. Going now to Deep Sleep");
            
            screen.force_full_update();
            msg_viewer.show(
              MsgViewer::INFO, 
              false, true, 
              "Deep Sleep", 
              "Timeout period exceeded (%d minutes). The device is now "
              "entering into Deep Sleep mode. Please press the WakeUp Button to restart.",
              light_sleep_duration);
            ESP::delay(1000);

            inkplate_platform.deep_sleep((gpio_num_t) 0, 0);
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
    
    touchscreen_isr_queue = xQueueCreate(          //create a queue to handle gpio event from isr
      20, sizeof(uint32_t));
    touchscreen_event_queue = xQueueCreate(          //create a queue to handle event from task
      20, sizeof(EventMgr::Event));

    touch_screen.set_app_isr_handler(touchscreen_isr_handler);

    TaskHandle_t xHandle = NULL;
    xTaskCreate(get_event_task, "GetEvent", 2000, nullptr, 10, &xHandle);

  #endif

  return true;
}

#endif
