// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.
//
//
// Touch Screen calibration algorithm from: https://www.embedded.com/how-to-calibrate-touch-screens
//

#if INKPLATE_6PLUS || TOUCH_TRIAL

#define __EVENT_MGR__ 1
#include "controllers/event_mgr.hpp"

#include <iostream>
#include <cmath>

#include "controllers/app_controller.hpp"
#include "viewers/page.hpp"
#include "models/config.hpp"

const char * EventMgr::event_str[8] = { "NONE",        "TAP",           "HOLD",         "SWIPE_LEFT", 
                                        "SWIPE_RIGHT", "PINCH_ENLARGE", "PINCH_REDUCE", "RELEASE"    };


#if EPUB_INKPLATE_BUILD


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
            event_mgr.set_position(x[0], y[0]);
            event_mgr.to_user_coord(x[0], y[0]);
            x_start = x[0];
            y_start = y[0];
          }
          else if (count == 2) {
            event_mgr.to_user_coord(x[0], y[0]);
            event_mgr.to_user_coord(x[1], y[1]);
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
              event_mgr.set_position(x[0], y[0]);
              event_mgr.to_user_coord(x[0], y[0]);
              x_end = x[0];
              y_end = y[0];
              if (DISTANCE > DIST_TRESHOLD) {
                state = State::SWIPING;
              }
            }
            else if (count == 2) {
              event_mgr.to_user_coord(x[0], y[0]);
              event_mgr.to_user_coord(x[1], y[1]);
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
              event_mgr.set_position(x[0], y[0]);
              event_mgr.to_user_coord(x[0], y[0]);
              x_end = x[0];
              y_end = y[0];
            }
            else if (count == 2) {
              event_mgr.to_user_coord(x[0], y[0]);
              event_mgr.to_user_coord(x[1], y[1]);
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
              event_mgr.to_user_coord(x[0], y[0]);
              event_mgr.to_user_coord(x[1], y[1]);
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
    switch (screen.get_orientation()) {
      case Screen::Orientation::BOTTOM:
        LOG_D("Bottom...");
        calib_point = {
          .x = { ((uint16_t)(Screen::get_width()  - 100)), ((uint16_t)(Screen::get_width()  / 2)), 100 },
          .y = { 100, ((uint16_t)(Screen::get_height() - 100)), ((uint16_t)(Screen::get_height() / 2)) }
        };
        break;

      case Screen::Orientation::TOP:
        LOG_D("Top...");
        calib_point = {
          .x = { 100, ((uint16_t)(Screen::get_width()  / 2)), ((uint16_t)(Screen::get_width()  - 100)) },
          .y = { ((uint16_t)(Screen::get_height() - 100)), 100, ((uint16_t)(Screen::get_height() / 2)) }
        };
        break;
    
      case Screen::Orientation::LEFT:
        LOG_D("Left...");
        calib_point = {
          .x = { ((uint16_t)(Screen::get_width()  - 100)), 100, ((uint16_t)(Screen::get_width()  / 2)) },
          .y = { ((uint16_t)(Screen::get_height() - 100)), ((uint16_t)(Screen::get_height() / 2)), 100 }
        };
        break;

      case Screen::Orientation::RIGHT:
        LOG_D("Right...");
        calib_point = {
          .x = { 100, ((uint16_t)(Screen::get_width()  - 100)), ((uint16_t)(Screen::get_width()  / 2)) },
          .y = { 100, ((uint16_t)(Screen::get_height() / 2)), ((uint16_t)(Screen::get_height() - 100)) }
        };
        break;
    }

    auto cross_hair = [](uint16_t x, uint16_t y) {
      page.put_highlight(Dim(41, 3), Pos(x - 20, y - 1));
      page.put_highlight(Dim(43, 5), Pos(x - 21, y - 2));
      page.put_highlight(Dim(45, 7), Pos(x - 22, y - 3));

      page.put_highlight(Dim(3, 41), Pos(x - 1, y - 20));
      page.put_highlight(Dim(5, 43), Pos(x - 2, y - 21));
      page.put_highlight(Dim(7, 45), Pos(x - 3, y - 22));
    };

    for (int i = 0; i < 3; i++) cross_hair(calib_point.x[i], calib_point.y[i]);

    Page::Format fmt = {
      .line_height_factor =   1.0,
      .font_index         =     1,
      .font_size          =    11,
      .indent             =     0,
      .margin_left        =     5,
      .margin_right       =     5,
      .margin_top         =     0,
      .margin_bottom      =     0,
      .screen_left        =    20,
      .screen_right       =    20,
      .screen_top         =     0,
      .screen_bottom      =     0,
      .width              =     0,
      .height             =     0,
      .vertical_align     =     0,
      .trim               =  true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::CENTER,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

    page.put_str_at("Touch Sensor Calibration", Pos(Screen::get_width() >> 1, (Screen::get_height() >> 1) - 150), fmt);
    page.put_str_at("Please TAP once on each crosshair to calibrate.", Pos(Screen::get_width() >> 1, (Screen::get_height() >> 1) - 100), fmt);

    calib_count = 0;
    page.paint();
  }

  void 
  EventMgr::to_user_coord(uint16_t & x, uint16_t & y)
  {
    uint16_t temp;
    Screen::Orientation orientation = screen.get_orientation();

    if (a == 0) {
      LOG_I("Calibration required!");
      switch (orientation) {
        case Screen::Orientation::BOTTOM:
          LOG_D("Bottom...");
          temp = y;
          y = ((uint32_t) (Screen::get_height() - 1) *                   x) / touch_screen.get_x_resolution();
          x = Screen::get_width() - ((uint32_t) (Screen::get_width()  - 1) * temp) / touch_screen.get_y_resolution();
          break;

        case Screen::Orientation::TOP:
          LOG_D("Top...");
          temp = y;
          y = Screen::get_height() - ((uint32_t) (Screen::get_height() - 1) * x) / touch_screen.get_x_resolution();
          x = ((uint32_t) (Screen::get_width()  - 1) *             temp ) / touch_screen.get_y_resolution();
          break;
      
        case Screen::Orientation::LEFT:
          LOG_D("Left...");
          x = Screen::get_width()  - ((uint32_t) (Screen::get_width()  - 1) * x) / touch_screen.get_x_resolution();
          y = Screen::get_height() - ((uint32_t) (Screen::get_height() - 1) * y) / touch_screen.get_y_resolution();
          break;

        case Screen::Orientation::RIGHT:
          LOG_D("Right...");
          x = ((uint32_t) (Screen::get_width()  - 1) * x) / touch_screen.get_x_resolution();
          y = ((uint32_t) (Screen::get_height() - 1) * y) / touch_screen.get_y_resolution();
          break;
      }
    }
    else {
      LOG_D("Coordinates to transform: [%u, %u]", x, y);

      uint16_t xd = (a * x + b * y + c) / divider;
      uint16_t yd = (d * x + e * y + f) / divider;

      switch (orientation) {
        case Screen::Orientation::BOTTOM:
          LOG_D("Bottom...");
          temp = y;
          y = xd;;
          x = Screen::get_width() - yd;
          break;

        case Screen::Orientation::TOP:
          LOG_D("Top...");
          temp = y;
          y = Screen::get_height() - xd;
          x = yd;
          break;

        case Screen::Orientation::LEFT:
          LOG_D("Left...");
          x = Screen::get_width()  - xd;
          y = Screen::get_height() - yd;
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

  bool EventMgr::calibration_event(const Event & event)
  {
    switch (event.kind) {
      case EventKind::TAP:
        if (calib_count < 3) {
          get_position(touch_point.x[calib_count], touch_point.y[calib_count]);
          calib_count++;
        }
        if (calib_count >= 3) {
          uint16_t temp;
          #define SWAP(i0, i1) { \
            temp = touch_point.x[i0]; touch_point.x[i0] = touch_point.x[i1]; touch_point.x[i1] = temp; \
            temp = touch_point.y[i0]; touch_point.y[i0] = touch_point.y[i1]; touch_point.y[i1] = temp; \
          }
          if (touch_point.y[0] > touch_point.y[2]) SWAP(0, 2);
          if (touch_point.y[0] > touch_point.y[1]) SWAP(0, 1);
          if (touch_point.y[1] > touch_point.y[2]) SWAP(1, 2);

          #undef SWAP

          #define X0 touch_point.x[0]
          #define X1 touch_point.x[1]
          #define X2 touch_point.x[2]
          #define Y0 touch_point.y[0]
          #define Y1 touch_point.y[1]
          #define Y2 touch_point.y[2]

          calib_point = {
            .x = { 100, ((uint16_t)(e_ink.get_height() - 100)), ((uint16_t)(e_ink.get_height()  / 2)) },
            .y = { 100, ((uint16_t)(e_ink.get_width()  /   2)), ((uint16_t)(e_ink.get_width() - 100)) }
          };

          #define XD0 calib_point.x[0]
          #define XD1 calib_point.x[1]
          #define XD2 calib_point.x[2]
          #define YD0 calib_point.y[0]
          #define YD1 calib_point.y[1]
          #define YD2 calib_point.y[2]

          LOG_D("Touch points: [%u, %u] [%u, %u] [%u, %u]",  X0,  Y0,  X1,  Y1,  X2,  Y2);
          LOG_D("Calib points: [%u, %u] [%u, %u] [%u, %u]", XD0, YD0, XD1, YD1, XD2, YD2);

          divider = ((X0 - X2) * (Y1 - Y2)) - ((X1 - X2) * (Y0 - Y2));
          
          a = (((XD0 - XD2) * ( Y1 -  Y2)) - ((XD1 - XD2) * (Y0 - Y2)));
          b = ((( X0 -  X2) * (XD1 - XD2)) - ((XD0 - XD2) * (X1 - X2)));
          c = ((Y0 * ((X2 * XD1) - (X1 * XD2))) +
               (Y1 * ((X0 * XD2) - (X2 * XD0))) +
               (Y2 * ((X1 * XD0) - (X0 * XD1))));
          d = (((YD0 - YD2) * ( Y1 -  Y2)) - ((YD1 - YD2) * (Y0 - Y2)));
          e = ((( X0 -  X2) * (YD1 - YD2)) - ((YD0 - YD2) * (X1 - X2)));
          f = ((Y0 * ((X2 * YD1) - (X1 * YD2))) +
               (Y1 * ((X0 * YD2) - (X2 * YD0))) +
               (Y2 * ((X1 * YD0) - (X0 * YD1))));

          LOG_D("Coefficients: a:%lld b:%lld c:%lld d:%lld e:%lld f:%lld divider:%lld", a, b, c, d, e, f, divider);

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

  void
  EventMgr::retrieve_calibration_values()
  {
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

  static gboolean
  mouse_event_callback(GtkWidget * event_box,
                       GdkEvent  * gdk_event,
                       gpointer    data) 
  {
    #define DISTANCE  (sqrt(pow(x_end - x_start, 2) + pow(y_end - y_start, 2)))
    #define DISTANCE2 ((x - x_start)  < 0 ? 0 : (x - x_start))

    static constexpr char const * TAG = "MouseEventCallback";

    const uint32_t MAX_DELAY_MS  =     4E3; // 4 seconds  
    const uint16_t DIST_TRESHOLD =      30; // treshold distance in pixels

    enum class State : uint8_t { NONE, WAIT_NEXT, HOLDING, SWIPING, PINCHING };

    static uint16_t 
      x_start    = 0, 
      y_start    = 0, 
      x_end      = 0, 
      y_end      = 0,
      last_dist  = 0;
    
    static State    state   = State::NONE;
    static bool     timeout = false;
    
    EventMgr::Event event = {
      .kind = EventMgr::EventKind::NONE,
      .x    = 0,
      .y    = 0,
      .dist = 0
    };

    uint8_t  count;
    uint16_t x = ((GdkEventButton *) gdk_event)->x;
    uint16_t y = ((GdkEventButton *) gdk_event)->y;
    
    if (gdk_event->type == GDK_BUTTON_RELEASE) {
      count = 0;
    }
    else if ((state != State::NONE) || (gdk_event->type != GDK_ENTER_NOTIFY)) {
      count = ((GdkEventButton *) gdk_event)->button == 3 ? 2 : 1 ;
    }
    else {
      count = 0;
    }


    LOG_D("State: %u", (uint8_t)state);

    switch (state) {
      case State::NONE:
        if (timeout) {
          timeout = false;
        }
        else if (((GdkEventButton *) gdk_event)->state & GDK_CONTROL_MASK) {
          state      = State::HOLDING;
          event.kind = EventMgr::EventKind::HOLD;
          event.x    = x;
          event.y    = y;
        }
        else if (count == 1) {
          state = State::WAIT_NEXT;
          event_mgr.set_position(x, y);
          x_start = x;
          y_start = y;
        }
        else if (count == 2) {
          x_start = x;
          y_start = y;
          event.dist = last_dist = DISTANCE2;
          state      = State::PINCHING;
        }
        break; 

      case State::WAIT_NEXT:
        if (count == 0) {
          state      = State::NONE;
          event.kind = EventMgr::EventKind::TAP;
          event.x    = x_start;
          event.y    = y_start;
        }
        else if (count == 1) {
          event_mgr.set_position(x, y);
          x_end = x;
          y_end = y;
          if (DISTANCE > DIST_TRESHOLD) {
            state = State::SWIPING;
          }
        }
        else if (count == 2) {
          event.dist = last_dist = DISTANCE2;
          state      = State::PINCHING;
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
          event.kind = (x_start < x_end) ? EventMgr::EventKind::SWIPE_RIGHT : 
                                           EventMgr::EventKind::SWIPE_LEFT; 
          state = State::NONE;
        }
        if (count == 1) {
          event_mgr.set_position(x, y);
          x_end = x;
          y_end = y;
        }
        else if (count == 2) {
          event.dist = last_dist = DISTANCE2;
          state      = State::PINCHING;
        }
        break;

      case State::PINCHING:
        if (count == 0) {
          event.kind = EventMgr::EventKind::RELEASE;
          state      = State::NONE;
        }
        else if (count == 2) {
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
      break;
    }

    if (event.kind != EventMgr::EventKind::NONE) {
      LOG_D("Input Event %s [%u, %u] (%u)...", 
            EventMgr::event_str[int(event.kind)], 
            event.x, event.y,
            event.dist);
      app_controller.input_event(event);
      app_controller.launch();
      return true;
    }

    return false;
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

          LOG_D("Light Sleep for %d minutes...", light_sleep_duration);
          ESP::delay(500);

          if (inkplate_platform.light_sleep(light_sleep_duration, TouchScreen::INTERRUPT_PIN, 0)) {

            app_controller.going_to_deep_sleep();
            
            LOG_D("Timed out on Light Sleep. Going now to Deep Sleep");
            
            screen.force_full_update();
            msg_viewer.show(
              MsgViewer::MsgType::INFO, 
              false, true, 
              "Deep Sleep", 
              "Timeout period exceeded (%d minutes). The device is now "
              "entering into Deep Sleep mode. Please press the WakeUp Button to restart.",
              light_sleep_duration);
            ESP::delay(1000);

            inkplate_platform.deep_sleep(TouchScreen::INTERRUPT_PIN, 0);
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
    
    retrieve_calibration_values();
  
    touchscreen_isr_queue = xQueueCreate(    //create a queue to handle gpio event from isr
      20, sizeof(uint32_t));
    touchscreen_event_queue = xQueueCreate(  //create a queue to handle event from task
      20, sizeof(EventMgr::Event));

    touch_screen.set_app_isr_handler(touchscreen_isr_handler);

    TaskHandle_t xHandle = NULL;
    xTaskCreate(get_event_task, "GetEvent", 2000, nullptr, 10, &xHandle);

  #endif

  return true;
}

#endif
