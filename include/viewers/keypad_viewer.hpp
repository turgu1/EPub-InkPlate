// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "controllers/event_mgr.hpp"

class KeypadViewer
{
  public:
    void          show(uint16_t value, Pos pos, uint16_t min_value, uint16_t max_value);
    bool         event(const EventMgr::Event & event);
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

    void update_value(bool show_cursor);
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