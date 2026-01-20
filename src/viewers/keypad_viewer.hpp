// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "controllers/event_mgr.hpp"
#include "viewers/page.hpp"
#include "models/fonts.hpp"

class KeypadViewer
{
  public:
    uint16_t get_value() { return client_value; }

  private:
    static constexpr char const * TAG = "KeypadViewer";
     
    static const uint8_t MAX_DIGITS       =  4;
    static const uint8_t KEY_COUNT        = 14;
    static const uint8_t FONT_SIZE        =  9;

    struct KeyLocation {
      Pos     pos;
      int8_t  value; // 0-9 => value, -1 => cancel, -2 => OK, -3 => BackSpace, -4 => Clear
    };


    static const uint8_t KEY_ADDED_WIDTH  = 50;
    static const uint8_t KEY_ADDED_HEIGHT = 30;

    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      KeyLocation * matrix[5][3];
      KeyLocation * current_key;
      KeyLocation * previous_key;
      uint8_t  line, col;
    #endif

    KeyLocation   key_locs[KEY_COUNT];
    Dim           keypad_dim;
    Pos           keypad_pos;
    Dim           key_dim, key_dim2;
    char          digits[MAX_DIGITS + 1];
    uint8_t       digits_count;
    Pos           field_pos;    // Where to put the number on screen
    uint16_t      client_value; // Computed value
    Page::Format  fmt;
    Font *        font;
    Font::Glyph * glyph;

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      int8_t get_key_val(uint16_t x, uint16_t y) {
        for (int i = 0; i < (KEY_COUNT - 1); i++) {
          if ((x >= key_locs[i].pos.x) &&
              (x <= key_locs[i].pos.x + key_dim.width) &&
              (y >= key_locs[i].pos.y) &&
              (y <= key_locs[i].pos.y + key_dim.height)) {
            return key_locs[i].value;
          }
        }
        if ((x >= key_locs[KEY_COUNT - 1].pos.x) &&
            (x <= key_locs[KEY_COUNT - 1].pos.x + key_dim2.width) &&
            (y >= key_locs[KEY_COUNT - 1].pos.y) &&
            (y <= key_locs[KEY_COUNT - 1].pos.y + key_dim2.height)) {
          return key_locs[KEY_COUNT - 1].value;
        }
        return 99; // Not found
      }
    #endif

    void update_value() {
      page.clear_region(Dim(keypad_dim.width - 6, key_dim.height - 6),
                        Pos(field_pos.x + 3, field_pos.y + 3));
      page.put_str_at(digits, 
                      Pos(field_pos.x + (keypad_dim.width >> 1), 
                          field_pos.y + (glyph->dim.height >> 1) + (key_dim.height >> 1)),
                      fmt);
    }

    void add_digit(uint8_t d) {
      if ((digits_count == 1) && (digits[0] == '0')) {
        digits[0] = '0' + d;
      }
      else if (digits_count < MAX_DIGITS) {
        digits[digits_count++] = '0' + d;
        digits[digits_count  ] = 0;
      }
    }

    void clear_digits() {
      digits[0] = '0';
      digits[1] =  0;
      digits_count = 1;
    }

    void remove_digit() {
      if (digits_count == 1) {
        digits[0] = '0';
      }
      else {
        digits[--digits_count] = 0;
      }
    }

    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      void update_highlight() {
        if (previous_key != current_key) {
          if (previous_key != nullptr) {
            if (previous_key->value == -1) { // Cancel key
              page.clear_highlight(Dim(key_dim2.width      - 2, key_dim2.height     - 2), 
                                   Pos(previous_key->pos.x + 1, previous_key->pos.y + 1));
              page.clear_highlight(Dim(key_dim2.width      - 4, key_dim2.height     - 4), 
                                   Pos(previous_key->pos.x + 2, previous_key->pos.y + 2));
              page.clear_highlight(Dim(key_dim2.width      - 6, key_dim2.height     - 6), 
                                   Pos(previous_key->pos.x + 3, previous_key->pos.y + 3));
            }
            else {
              page.clear_highlight(Dim(key_dim.width       - 2, key_dim.height      - 2), 
                                   Pos(previous_key->pos.x + 1, previous_key->pos.y + 1));
              page.clear_highlight(Dim(key_dim.width       - 4, key_dim.height      - 4), 
                                   Pos(previous_key->pos.x + 2, previous_key->pos.y + 2));
              page.clear_highlight(Dim(key_dim.width       - 6, key_dim.height      - 6), 
                                   Pos(previous_key->pos.x + 3, previous_key->pos.y + 3));
            }
          }
          if (current_key->value == -1) { // Cancel key
            page.put_highlight(Dim(key_dim2.width     - 2, key_dim2.height    - 2), 
                               Pos(current_key->pos.x + 1, current_key->pos.y + 1));
            page.put_highlight(Dim(key_dim2.width     - 4, key_dim2.height    - 4), 
                               Pos(current_key->pos.x + 2, current_key->pos.y + 2));
            page.put_highlight(Dim(key_dim2.width     - 6, key_dim2.height    - 6), 
                               Pos(current_key->pos.x + 3, current_key->pos.y + 3));
          }
          else {
            page.put_highlight(Dim(key_dim.width      - 2, key_dim.height     - 2), 
                               Pos(current_key->pos.x + 1, current_key->pos.y + 1));
            page.put_highlight(Dim(key_dim.width      - 4, key_dim.height     - 4), 
                               Pos(current_key->pos.x + 2, current_key->pos.y + 2));
            page.put_highlight(Dim(key_dim.width      - 6, key_dim.height     - 6), 
                               Pos(current_key->pos.x + 3, current_key->pos.y + 3));
          }
        }
      }
    #endif

  public:

    void show(uint16_t value, const char * caption) {

      client_value = value;
      if (client_value > 9999) client_value = 9999;
      
      int_to_str(client_value, digits, 5);

      digits_count = strlen(digits);

      //LOG_I("Digits: %s", digits);

      font         =  fonts.get(1);
      glyph        =  font->get_glyph('0', FONT_SIZE);

      key_dim  = Dim(glyph->dim.width  + KEY_ADDED_WIDTH, 
                     glyph->dim.height + KEY_ADDED_HEIGHT);
      key_dim2 = Dim((key_dim.width << 1) + 2, 
                      key_dim.height);

      LOG_D("Key Dim: [%d, %d], Dim2: [%d, %d]",
            key_dim.width,  key_dim.height,
            key_dim2.width, key_dim2.height);

      keypad_dim = Dim(((key_dim.width + 2) * 3) - 2, (key_dim.height + 2) * 7);

      keypad_pos = Pos((Screen::get_width()  >> 1) - (keypad_dim.width  >> 1), 
                       (Screen::get_height() >> 1) - (keypad_dim.height >> 1));

      field_pos = Pos(keypad_pos.x, keypad_pos.y + key_dim.height + 2);

      LOG_D("Keypad Dim: [%d, %d], Pos: [%d, %d]",
        keypad_dim.width, keypad_dim.height,
        keypad_pos.x,     keypad_pos.y);

      // Show keypad on screen

      fmt = {
        .line_height_factor =   1.0,
        .font_index         =     1,
        .font_size          = FONT_SIZE,
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

      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
        page.start(fmt);
      #endif

      // The large rectangle into which the keypad will be drawn

      page.clear_region(
        Dim(keypad_dim.width + 26, keypad_dim.height + 26),
        Pos(keypad_pos.x     - 13, keypad_pos.y      - 13));
      page.put_highlight(
        Dim(keypad_dim.width + 20, keypad_dim.height + 20),
        Pos(keypad_pos.x     - 10, keypad_pos.y      - 10));

      constexpr int8_t values[14] = {
        7, 8, 9, 4, 5, 6, 1, 2, 3, -4, 0, -3, -2, -1
      };
      constexpr char labels[12] = "789456123 0";

      page.put_str_at(caption, 
                      Pos(Screen::get_width() >> 1, 
                          keypad_pos.y + (glyph->dim.height >> 1) + (key_dim.height >> 1)), 
                      fmt);

      page.put_highlight(
        Dim(keypad_dim.width, key_dim.height),
        Pos(field_pos.x,      field_pos.y   ));
      page.put_highlight(
        Dim(keypad_dim.width - 2, key_dim.height - 2),
        Pos(field_pos.x + 1,      field_pos.y + 1   ));
      page.put_highlight(
        Dim(keypad_dim.width - 4, key_dim.height - 4),
        Pos(field_pos.x + 2,      field_pos.y + 2   ));

      update_value();

      Pos the_pos = Pos(keypad_pos.x, keypad_pos.y + (key_dim.height << 1) + 4);

      #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
        line = col = 0;
      #endif

      for (int i = 0; i < 14; i++) {
        key_locs[i] = { .pos = the_pos, .value = values[i] };

        #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
          matrix[line][col] = &key_locs[i];
          if (++col >= 3) {
            col = 0;
            line++;
          }
        #endif

        LOG_D("Key %d pos: [%d, %d]", i, the_pos.x, the_pos.y);

        switch (values[i]) {
          case 0:
          case 1: 
          case 2: 
          case 3: 
          case 4: 
          case 5: 
          case 6: 
          case 7: 
          case 8: 
          case 9:
            page.put_highlight(key_dim, the_pos);
            page.put_char_at(labels[i], 
                             Pos(the_pos.x + (key_dim.width     >> 1) - (glyph->dim.width >> 1), 
                                 the_pos.y + (glyph->dim.height >> 1) + (key_dim.height   >> 1)), 
                             fmt);
            break;
          case -1: // Cancel
            page.put_highlight(key_dim2, the_pos);
            #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
              page.put_highlight(Dim(key_dim2.width - 2, key_dim2.height - 2), 
                                 Pos(the_pos.x      + 1, the_pos.y       + 1));
            #endif
            page.put_str_at("CANCEL", 
                            Pos(the_pos.x + (key_dim2.width    >> 1), 
                                the_pos.y + (glyph->dim.height >> 1) + (key_dim.height >> 1)), 
                            fmt);
            break;
          case -2: // OK
            page.put_highlight(key_dim, the_pos);
            #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
              page.put_highlight(Dim(key_dim.width - 2, key_dim.height - 2), 
                                 Pos(the_pos.x     + 1, the_pos.y      + 1));
            #endif
            page.put_str_at("OK", 
                            Pos(the_pos.x + (key_dim.width     >> 1), 
                                the_pos.y + (glyph->dim.height >> 1) + (key_dim.height >> 1)), 
                            fmt);
            break;
          case -3: // Backspace
            page.put_highlight(key_dim, the_pos);
            page.put_str_at("BSP", 
                            Pos(the_pos.x + (key_dim.width     >> 1), 
                                the_pos.y + (glyph->dim.height >> 1) + (key_dim.height >> 1)), 
                            fmt);
            break;
          case -4: // Clear
            page.put_highlight(key_dim, the_pos);
            page.put_str_at("CLR", 
                            Pos(the_pos.x + (key_dim.width     >> 1), 
                                the_pos.y + (glyph->dim.height >> 1) + (key_dim.height >> 1)), 
                            fmt);
            break;
        }
        if (((i + 1) % 3) == 0) {
          the_pos.x = keypad_pos.x;
          the_pos.y += key_dim.height + 2;
        }
        else {
          the_pos.x += key_dim.width + 2;
        }
      }

      #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
        matrix[4][2] = matrix[4][1]; 
        line = col = 1;
        previous_key = nullptr;
        current_key = matrix[1][1];
        update_highlight();
        previous_key = current_key;
      #endif

      update_value();
      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
        page.paint(false);
      #endif
    }

    /**
     * @brief Event processing
     * 
     * @param event Event coming from the user interaction
     * @return true The Keypad must keep control of events
     * @return false Events processing is complete
     */
    bool event(const EventMgr::Event & event) {

      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
        if (event.kind == EventMgr::EventKind::TAP) {
          int8_t value = get_key_val(event.x, event.y);
          if (value != 99) {
            if (value >= 0) add_digit(value);
            else if (value == -1) return false;
            else if (value == -2) {
              client_value = atoi(digits);
              return false;
            }
            else if (value == -3) remove_digit();
            else if (value == -4) clear_digits();
          }
        }
        page.start(fmt);
        update_value();
        page.paint(false);

        return true;
      #else
        switch (event.kind) {
          #if EXTENDED_CASE
            case EventMgr::EventKind::PREV:
          #else
            case EventMgr::EventKind::DBL_PREV:
          #endif
            col = (col == 0) ? 2 : col - 1;
            current_key = matrix[line][col];
            break;

          #if EXTENDED_CASE
            case EventMgr::EventKind::NEXT:
          #else
            case EventMgr::EventKind::DBL_NEXT:
          #endif
            col = (col == 2) ? 0 : col + 1;
            current_key = matrix[line][col];
            break;

          #if EXTENDED_CASE
            case EventMgr::EventKind::DBL_PREV:
          #else
            case EventMgr::EventKind::PREV:
          #endif
            line = (line == 0) ? 4 : line - 1;
            current_key = matrix[line][col];
            break;

          #if EXTENDED_CASE
            case EventMgr::EventKind::DBL_NEXT:
          #else
            case EventMgr::EventKind::NEXT:
          #endif
            line = (line == 4) ? 0 : line + 1;
            current_key = matrix[line][col];
            break;

          case EventMgr::EventKind::SELECT:
          {
              int8_t value = current_key->value;
              if (value != 99) {
                if (value >= 0) add_digit(value);
                else if (value == -1) return false;
                else if (value == -2) {
                  client_value = atoi(digits);
                  return false;
                }
                else if (value == -3) remove_digit();
                else if (value == -4) clear_digits();
              }
            }
            break;

          case EventMgr::EventKind::DBL_SELECT:
            return false;
            break;

          default:
            break;
        }      

        page.start(fmt);
        update_value();
        update_highlight();
        previous_key = current_key;
        page.paint(false);

        return true;
      #endif
    }
    
};

#if __KEYPAD_VIEWER__
  KeypadViewer keypad_viewer;
#else
  extern KeypadViewer keypad_viewer;
#endif

#if 0

Keypad:

   +-----------------+
   |     Caption     |
   +-----------------+
   |      Value      |
   +-----+-----+-----+
   |  7  |  8  |  9  |
   +-----+-----+-----+
   |  4  |  5  |  6  |
   +-----+-----+-----+
   |  1  |  2  |  3  |
   +-----+-----+-----+
   | CLR |  0  | BSP |
   +-----+-----+-----+
   | OK  |  CANCEL   |
   +-----+-----------+

#endif