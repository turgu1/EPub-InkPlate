// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __SCREEN_HPP__
#define __SCREEN_HPP__

#include "noncopyable.hpp"
#include "eink.hpp"

/**
 * @brief Low level logical Screen display
 * 
 * This class implements the low level methods required to paint
 * on the display. Under the InlPlate6, it is using the EInk display driver. 
 * 
 * This is a singleton. It cannot be instanciated elsewhere. It is not 
 * instanciated in the heap. This is reinforced by the C++ construction
 * below. It also cannot be copied through the NonCopyable derivation.
 */

class Screen : NonCopyable
{
  public:
    static uint16_t WIDTH;
    static uint16_t HEIGHT;
    static enum Orientation : int8_t { O_LEFT, O_RIGHT, O_BOTTOM } orientation;
    static constexpr uint16_t RESOLUTION            = 166;  ///< Pixels per inch
    static constexpr uint8_t  HIGHLIGHT_COLOR       = 1;
    static constexpr uint8_t  WHITE_COLOR           = 0;
    static constexpr int8_t   PARTIAL_COUNT_ALLOWED = 6;
    
    void draw_bitmap(const unsigned char * bitmap_data, 
                     uint16_t width, uint16_t height, 
                     int16_t x, int16_t y);
    void  draw_glyph(const unsigned char * bitmap_data, 
                     uint16_t width, uint16_t height, uint16_t pitch,
                     int16_t x, int16_t y);
    void draw_rectangle(uint16_t width, uint16_t height, 
                        int16_t x, int16_t y,
                        uint8_t color);
    void colorize_region(uint16_t width, uint16_t height, 
                         int16_t x, int16_t y, uint8_t color);

    inline void clear()  { EInk::clear_bitmap(*frame_buffer); }
    inline void update(bool no_full = false) { 
      if (no_full) {
        e_ink.partial_update(*frame_buffer);
        partial_count = 0;
      }
      else {
        if (partial_count <= 0) {
          e_ink.clean();
          e_ink.update(*frame_buffer);
          partial_count = PARTIAL_COUNT_ALLOWED;
        }
        else {
          e_ink.partial_update(*frame_buffer);
          partial_count--;
        }
      }
    }

  private:
    static constexpr char const * TAG = "Screen";
    static const uint8_t LUT1BIT[8];
    static const uint8_t LUT1BIT_INV[8];

    static Screen singleton;
    Screen() : partial_count(0) { };

    EInk::Bitmap1Bit * frame_buffer;
    int8_t partial_count;

    // inline void set_pixel(uint32_t col, uint32_t row, uint8_t color) {
    //   uint8_t * temp = &(*frame_buffer)[EInk::BITMAP_SIZE_3BIT - (EInk::LINE_SIZE_3BIT * (col + 1)) + (row >> 1)];
    //   *temp = col & 1 ? (*temp & 0x70) | color : (*temp & 0x07) | (color << 4);
    // }

    inline void set_pixel_o_left(uint32_t col, uint32_t row, uint8_t color) {
      uint8_t * temp = &(*frame_buffer)[EInk::BITMAP_SIZE_1BIT - (EInk::LINE_SIZE_1BIT * (col + 1)) + (row >> 3)];
      if (color == 1)
        *temp = *temp | LUT1BIT_INV[row & 7];
      else
        *temp = (*temp & ~LUT1BIT_INV[row & 7]);
    }

    inline void set_pixel_o_right(uint32_t col, uint32_t row, uint8_t color) {
      uint8_t * temp = &(*frame_buffer)[(EInk::LINE_SIZE_1BIT * (col + 1)) - (row >> 3) - 1];
      if (color == 1)
        *temp = *temp | LUT1BIT[row & 7];
      else
        *temp = (*temp & ~LUT1BIT[row & 7]);
    }

    inline void set_pixel_o_bottom(uint32_t col, uint32_t row, uint8_t color) {
      uint8_t * temp = &(*frame_buffer)[EInk::LINE_SIZE_1BIT * row + (col >> 3)];
      if (color == 1)
        *temp = *temp | LUT1BIT_INV[col & 7];
      else
        *temp = (*temp & ~LUT1BIT_INV[col & 7]);
    }

  public:
    static Screen & get_singleton() noexcept { return singleton; }
    void setup();
    void set_orientation(Orientation orient) {
      orientation = orient;
      if ((orientation == O_LEFT) || (orientation == O_RIGHT)) {
        WIDTH  = 600;
        HEIGHT = 800;
      }
      else {
        WIDTH  = 800;
        HEIGHT = 600;
      }
    }
};

#if __SCREEN__
  Screen & screen = Screen::get_singleton();
#else
  extern Screen & screen;
#endif

#endif