// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "inkplate_platform.hpp"
#include "non_copyable.hpp"
#include "picture.hpp"

/**
 * @brief Low level logical Screen display
 *
 * This class implements the low level methods required to paint
 * on the display. Under the InkPlate6, it is using the EInk display driver.
 *
 * This is a singleton. It cannot be instanciated elsewhere. It is not
 * instanciated in the heap. This is reinforced by the C++ construction
 * below. It also cannot be copied through the NonCopyable derivation.
 */

class Screen : NonCopyable {
public:
  #if INKPLATE_10
    static constexpr int8_t IDENT                 = 2;
    static constexpr int8_t PARTIAL_COUNT_ALLOWED = 10;
    static constexpr uint16_t RESOLUTION          = 150; ///< Pixels per inch
  #elif INKPLATE_6
    static constexpr int8_t IDENT                 = 1;
    static constexpr int8_t PARTIAL_COUNT_ALLOWED = 10;
    static constexpr uint16_t RESOLUTION          = 166; ///< Pixels per inch
  #elif INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    static constexpr int8_t IDENT                  = 3;
    static constexpr int16_t PARTIAL_COUNT_ALLOWED = 10;
    static constexpr uint16_t RESOLUTION           = 212; ///< Pixels per inch
  #endif
  enum class Orientation : int8_t {
    LEFT, RIGHT, BOTTOM, TOP
  };
  enum class PixelResolution : int8_t {
    ONE_BIT, THREE_BITS
  };

  enum Color {
    WHITE = 0, BLACK = 7
  };

  void drawPicture(PicturePtr &picture, Pos pos);
  void drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch);
  void drawRectangle(Dim dim, Pos pos, uint8_t color);
  void drawRoundRectangle(Dim dim, Pos pos, uint8_t color);
  void colorizeRegion(Dim dim, Pos pos, uint8_t color);

  void lowColorize1Bit(Dim dim, Pos pos, uint8_t color);
  void lowColorize3Bit(Dim dim, Pos pos, uint8_t color);

  inline void clear() {
    if (pixelResolution == PixelResolution::ONE_BIT) {
      frameBuffer1Bit->clear();
    } else {
      frameBuffer3Bit->clear();
    }
  }

  inline void update(bool noFull = false) {
    if (pixelResolution == PixelResolution::ONE_BIT) {
      if (noFull) {
        e_ink.partial_update(*frameBuffer1Bit);
        partialCount = 0;
      } else {
        if (partialCount <= 0) {
          // e_ink.clean();
          e_ink.update(*frameBuffer1Bit);
          partialCount = PARTIAL_COUNT_ALLOWED;
        } else {
          e_ink.partial_update(*frameBuffer1Bit);
          partialCount--;
        }
      }
    } else {
      e_ink.update(*frameBuffer3Bit);
    }
  }

private:
  static constexpr char const *TAG = "Screen";
  static const uint8_t LUT1BIT[8];
  static const uint8_t LUT1BIT_INV[8];

  static uint16_t width;
  static uint16_t height;

  static Screen singleton;

  int16_t partialCount{0};
  FrameBuffer1Bit *frameBuffer1Bit{nullptr};
  FrameBuffer3Bit *frameBuffer3Bit{nullptr};
  PixelResolution pixelResolution{PixelResolution::ONE_BIT};
  Orientation orientation{Orientation::LEFT};

  enum class Corner : uint8_t {
    TOP_LEFT, TOP_RIGHT, LOWER_LEFT, LOWER_RIGHT
  };

  Screen() = default;

  void drawArc(uint16_t xMid, uint16_t yMid, uint8_t radius, Corner corner, uint8_t color);

  inline void set_pixel_o_left_1bit(uint32_t col, uint32_t row, uint8_t color) {
    uint8_t *temp =
        &(frameBuffer1Bit->get_data())[frameBuffer1Bit->get_data_size() -
                                       (frameBuffer1Bit->get_line_size() * (col + 1)) + (row >> 3)];
    if (color == 1)
      *temp = *temp | LUT1BIT_INV[row & 7];
    else
      *temp = (*temp & ~LUT1BIT_INV[row & 7]);
  }

  inline void set_pixel_o_right_1bit(uint32_t col, uint32_t row, uint8_t color) {
    uint8_t *temp =
        &(frameBuffer1Bit
              ->get_data())[(frameBuffer1Bit->get_line_size() * (col + 1)) - (row >> 3) - 1];
    if (color == 1)
      *temp = *temp | LUT1BIT[row & 7];
    else
      *temp = (*temp & ~LUT1BIT[row & 7]);
  }

  inline void set_pixel_o_bottom_1bit(uint32_t col, uint32_t row, uint8_t color) {
    uint8_t *temp =
        &(frameBuffer1Bit->get_data())[frameBuffer1Bit->get_line_size() * row + (col >> 3)];
    if (color == 1)
      *temp = *temp | LUT1BIT_INV[col & 7];
    else
      *temp = (*temp & ~LUT1BIT_INV[col & 7]);
  }

  inline void set_pixel_o_top_1bit(uint32_t col, uint32_t row, uint8_t color) {
    uint8_t *temp =
        &(frameBuffer1Bit->get_data())[frameBuffer1Bit->get_data_size() -
                                       (frameBuffer1Bit->get_line_size() * row) - (col >> 3)];
    if (color == 1)
      *temp = *temp | LUT1BIT[col & 7];
    else
      *temp = (*temp & ~LUT1BIT[col & 7]);
  }

  inline void set_pixel_o_left_3bit(uint32_t col, uint32_t row, uint8_t color) {
    uint8_t *temp =
        &(frameBuffer3Bit->get_data())[frameBuffer3Bit->get_data_size() -
                                       (frameBuffer3Bit->get_line_size() * (col + 1)) + (row >> 1)];
    if (row & 1)
      *temp = (*temp & 0xF0) | color;
    else
      *temp = (*temp & 0x0F) | (color << 4);
  }

  inline void set_pixel_o_right_3bit(uint32_t col, uint32_t row, uint8_t color) {
    uint8_t *temp =
        &(frameBuffer3Bit
              ->get_data())[(frameBuffer3Bit->get_line_size() * (col + 1)) - (row >> 1) - 1];
    if (row & 1)
      *temp = (*temp & 0x0F) | (color << 4);
    else
      *temp = (*temp & 0xF0) | color;
  }

  inline void set_pixel_o_bottom_3bit(uint32_t col, uint32_t row, uint8_t color) {
    uint8_t *temp =
        &(frameBuffer3Bit->get_data())[frameBuffer3Bit->get_line_size() * row + (col >> 1)];
    if (col & 1)
      *temp = (*temp & 0xF0) | color;
    else
      *temp = (*temp & 0x0F) | (color << 4);
  }

  inline void set_pixel_o_top_3bit(uint32_t col, uint32_t row, uint8_t color) {
    uint8_t *temp =
        &(frameBuffer3Bit->get_data())[frameBuffer3Bit->get_data_size() -
                                       (frameBuffer3Bit->get_line_size() * row) - (col >> 1)];
    if (col & 1)
      *temp = (*temp & 0x0F) | (color << 4);
    else
      *temp = (*temp & 0xF0) | color;
  }

public:
  static Screen &getSingleton() noexcept { return singleton; }
  void setup(PixelResolution resolution, Orientation orientation);
  void setPixelResolution(PixelResolution resolution, bool force = false);
  void setOrientation(Orientation orient);
  inline Orientation getOrientation() { return orientation; }
  inline PixelResolution getPixelResolution() { return pixelResolution; }
  inline void forceFullUpdate() { partialCount = 0; }

  // #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
  //   void to_user_coord(uint16_t &x, uint16_t &y);
  // #endif

  inline static uint16_t getWidth() { return width; }
  inline static uint16_t getHeight() { return height; }
};

#if __SCREEN__
  Screen &screen = Screen::getSingleton();
#else
  extern Screen &screen;
#endif
