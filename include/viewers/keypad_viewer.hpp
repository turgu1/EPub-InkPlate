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
    uint16_t get_value() { return value; }

  private:
    static constexpr char const * TAG = "KeypadViewer";
     
    static const uint8_t MAX_DIGITS       =  4;
    static const uint8_t KEY_COUNT        = 13;
    static const uint8_t FONT_SIZE        = 12;
    static const uint8_t KEY_ADDED_WIDTH  = 30;
    static const uint8_t KEY_ADDED_HEIGHT = 20;

    struct KeyLocation {
      Pos     pos;
      int8_t  value; // 0-9 => value, -1 => cancel, -2 => OK, -3 => BackSpace
    };

    KeyLocation key_locs[KEY_COUNT];
    Dim         key_dim, key_dim2;
    char        digits[MAX_DIGITS + 1];
    uint8_t     digits_count;
    uint16_t    min_val, max_val;
    Pos         field_pos;   // Where to put the number on screen
    uint16_t    value;       // Computed value


    void update_value(bool show_cursor)
    {
      page.paint(false);
    }

  public:

    void show(uint16_t value, Pos pos, uint16_t min_value, uint16_t max_value) {

      int v = value;
      if (value <= 9999) {
        int_to_str(v, digits, 5);
      }
      else {
        digits[0] = 0;
      }

      digits_count = strlen(digits);
      field_pos    = pos;
      min_val      = min_value;
      max_val      = max_value;

      Font *        font             =  fonts.get(5);
      Font::Glyph * glyph            =  font->get_glyph('0', FONT_SIZE);

      Pos           keypad_pos;

      Dim           keypad_dim = { (uint16_t)(((glyph->dim.width  + 22) * 3) - 2), 
                                   (uint16_t)(((glyph->dim.height + 22) * 5) - 2) };

      keypad_pos.x = (screen.WIDTH >> 1) - (keypad_dim.width >> 1);
      if ((field_pos.y + 40 + keypad_dim.height) > screen.HEIGHT) {
        keypad_pos.y = field_pos.y - keypad_dim.height - glyph->dim.height - 30;
      }
      else {
        keypad_pos.y = field_pos.y + 30;
      }

      // Show keypad on screen

        Page::Format fmt = {
        .line_height_factor =   1.0,
        .font_index         =     5,
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
        .align              = CSS::Align::LEFT,
        .text_transform     = CSS::TextTransform::NONE,
        .display            = CSS::Display::INLINE
      };

      page.start(fmt);

      // The large rectangle into which the keypad will be drawn

      page.clear_region(
        Dim(keypad_dim.width + 20, keypad_dim.height + 20),
        Pos(keypad_pos.x - 10, keypad_pos.y - 10));

      static int8_t values[14] = {
        7, 8, 9, 4, 5, 6, 1, 2, 3, 99, 0, -3, -2, -1
      };
      static char labels[12] = "789456123 0";

      Pos the_pos = keypad_pos;
      key_dim  = { (uint16_t)(glyph->dim.width  + KEY_ADDED_WIDTH), 
                   (uint16_t)(glyph->dim.height + KEY_ADDED_HEIGHT) };
      key_dim2 = { (uint16_t)((key_dim.width * 3) >> 1), key_dim.height };

      int k = 0;
      for (int i = 0; i < 14; i++) {
        if (values[i] != 99) {
          key_locs[k] = { .pos = the_pos, .value = values[i] };
          LOG_D("Key %d pos: [%d, %d]", i, the_pos.x, the_pos.y);
        } 
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
            page.put_char_at(labels[i], Pos(the_pos.x + 10, the_pos.y + glyph->dim.height + 10), fmt);
            break;
          case 99:
            break;
          case -1: // Cancel
            page.put_highlight(key_dim2, the_pos);
            page.put_char_at('C', Pos(the_pos.x + 10, the_pos.y + glyph->dim.height + 10), fmt);
            break;
          case -2: // OK
            page.put_highlight(key_dim2, the_pos);
            page.put_char_at('O', Pos(the_pos.x + 10, the_pos.y + glyph->dim.height + 10), fmt);
            break;
          case -3: // Backspace
            page.put_highlight(key_dim, the_pos);
            page.put_char_at('B', Pos(the_pos.x + 10, the_pos.y + glyph->dim.height + 10), fmt);
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

      update_value(true);
      page.paint(false);
    }

    bool event(const EventMgr::Event & event) {
      return true;
    }
};

#if __KEYPAD_VIEWER__
  KeypadViewer keypad_viewer;
#else
  extern KeypadViewer keypad_viewer;
#endif

#if 0

Keypad:

   +-----+-----+-----+
   |  7  |  8  |  9  |
   +-----+-----+-----+
   |  4  |  5  |  6  |
   +-----+-----+-----+
   |  1  |  2  |  3  |
   +-----+-----+-----+
   |     |  0  | BSP |
   +-----+--+--+-----+
   | CANCEL |   OK   |
   +--------+--------+

#endif