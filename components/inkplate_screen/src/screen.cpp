// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// Using GTK on Linux to emulate InkPlate screen
//
// As all GTK related code is located in this module, we also implement
// some part of the event manager code here...

#define __SCREEN__ 1
#include "screen.hpp"

#include "esp.hpp"

#include <iomanip>

#define BYTES_PER_PIXEL 3

Screen Screen::singleton;

uint16_t Screen::width;
uint16_t Screen::height;

const uint8_t Screen::LUT1BIT[8]     = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
const uint8_t Screen::LUT1BIT_INV[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

#define SELECT(resolution)                                                                         \
  if (orientation == Orientation::LEFT) {                                                          \
    CODE(resolution, Left);                                                                        \
  } else if (orientation == Orientation::RIGHT) {                                                  \
    CODE(resolution, Right);                                                                       \
  } else if (orientation == Orientation::BOTTOM) {                                                 \
    CODE(resolution, Bottom);                                                                      \
  } else {                                                                                         \
    CODE(resolution, Top);                                                                         \
  }

auto Screen::drawPicture(PicturePtr &picture, Pos pos) -> void {

  auto dim        = picture->getDim();
  auto bitmapData = picture->getBitmap();

  if (pos.x > width) pos.x = 0;
  if (pos.y > height) pos.y = 0;

  uint32_t xMax = pos.x + dim.width;
  uint32_t yMax = pos.y + dim.height;

  if (yMax > height) yMax = height;
  if (xMax > width) xMax = width;

  if (pixelResolution == PixelResolution::ONE_BIT) {
    static int16_t err[1201]; // This is the maximum width of all Inkplate devices + 1
    int16_t error;
    memset(err, 0, 1201 * 2);

    #define CODE(resolution, orientation)                                                          \
      for (int j = pos.y, q = 0; j < yMax; j++, q++) {                                             \
        for (int i = pos.x, p = q * dim.width, k = 0; i < (xMax - 1); i++, p++, k++) {             \
          int32_t v = bitmapData[p] + err[k + 1];                                                  \
          if (v > 128) {                                                                           \
            error = (v - 255);                                                                     \
            setPixelO##orientation##resolution(i, j, 0);                                           \
          } else {                                                                                 \
            error = v;                                                                             \
            setPixelO##orientation##resolution(i, j, 1);                                           \
          }                                                                                        \
          if (k != 0) {                                                                            \
            err[k - 1] += error / 8;                                                               \
          }                                                                                        \
          err[k] += 3 * error / 8;                                                                 \
          err[k + 1] = error / 8;                                                                  \
          err[k + 2] += 3 * error / 8;                                                             \
        }                                                                                          \
      }

    SELECT(1Bit);
    #undef CODE
  } else {
    #define CODE(resolution, orientation)                                                          \
      for (int j = pos.y, q = 0; j < yMax; j++, q++) {                                             \
        for (int i = pos.x, p = q * dim.width; i < xMax; i++, p++) {                               \
          int8_t v = bitmapData[p] >> 5;                                                           \
          setPixelO##orientation##resolution(i, j, v);                                             \
        }                                                                                          \
      }
    SELECT(3Bit);
    #undef CODE
  }
}

auto Screen::drawRectangle(Dim dim, Pos pos, uint8_t color) -> void {
  uint32_t xMax = pos.x + dim.width;
  uint32_t yMax = pos.y + dim.height;

  if (yMax > height) yMax = height;
  if (xMax > width) xMax = width;

  #define CODE(resolution, orientation)                                                            \
    for (int i = pos.x; i < xMax; i++) {                                                           \
      setPixelO##orientation##resolution(i, pos.y, color);                                         \
      setPixelO##orientation##resolution(i, yMax - 1, color);                                      \
    }                                                                                              \
    for (int j = pos.y; j < yMax; j++) {                                                           \
      setPixelO##orientation##resolution(pos.x, j, color);                                         \
      setPixelO##orientation##resolution(xMax - 1, j, color);                                      \
    }

  if (pixelResolution == PixelResolution::ONE_BIT) {
    color = color == Color::BLACK ? 1 : 0;
    SELECT(1Bit);
  } else {
    #if INKPLATE_6PLUS | INKPLATE_6PLUS_V2 | INKPLATE_6FLICK
      color = color == Color::BLACK ? 0 : 7;
    #endif

    SELECT(3Bit);
  }

  #undef CODE
}

auto Screen::drawArc(uint16_t xMid, uint16_t yMid, uint8_t radius, Corner corner, uint8_t color)
    -> void {
  int16_t f     = 1 - radius;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * radius;
  int16_t x     = 0;
  int16_t y     = radius;

  #define CODE(resolution, orientation)                                                            \
    while (x < y) {                                                                                \
      if (f >= 0) {                                                                                \
        y--;                                                                                       \
        ddF_y += 2;                                                                                \
        f += ddF_y;                                                                                \
      }                                                                                            \
      x++;                                                                                         \
      ddF_x += 2;                                                                                  \
      f += ddF_x;                                                                                  \
                                                                                                   \
      switch (corner) {                                                                            \
      case Corner::TOP_LEFT:                                                                       \
        setPixelO##orientation##resolution(xMid - x, yMid - y, color);                             \
        setPixelO##orientation##resolution(xMid - y, yMid - x, color);                             \
        break;                                                                                     \
                                                                                                   \
      case Corner::TOP_RIGHT:                                                                      \
        setPixelO##orientation##resolution(xMid + x, yMid - y, color);                             \
        setPixelO##orientation##resolution(xMid + y, yMid - x, color);                             \
        break;                                                                                     \
                                                                                                   \
      case Corner::LOWER_LEFT:                                                                     \
        setPixelO##orientation##resolution(xMid - x, yMid + y, color);                             \
        setPixelO##orientation##resolution(xMid - y, yMid + x, color);                             \
        break;                                                                                     \
                                                                                                   \
      case Corner::LOWER_RIGHT:                                                                    \
        setPixelO##orientation##resolution(xMid + x, yMid + y, color);                             \
        setPixelO##orientation##resolution(xMid + y, yMid + x, color);                             \
        break;                                                                                     \
      }                                                                                            \
    }

  if (pixelResolution == PixelResolution::ONE_BIT) {
    SELECT(1Bit);
  } else {
    SELECT(3Bit);
  };

  #undef CODE
}

void Screen::drawRoundRectangle(Dim dim, Pos pos,
                                uint8_t color) //, bool show)
{
  int16_t xMax = pos.x + dim.width;
  int16_t yMax = pos.y + dim.height;

  if (yMax > height) yMax = height;
  if (xMax > width) xMax = width;

  #define CODE(resolution, orientation)                                                            \
    for (int i = pos.x + 10; i < xMax - 10; i++) {                                                 \
      setPixelO##orientation##resolution(i, pos.y, color);                                         \
      setPixelO##orientation##resolution(i, yMax - 1, color);                                      \
    }                                                                                              \
    for (int j = pos.y + 10; j < yMax - 10; j++) {                                                 \
      setPixelO##orientation##resolution(pos.x, j, color);                                         \
      setPixelO##orientation##resolution(xMax - 1, j, color);                                      \
    }

  if (pixelResolution == PixelResolution::ONE_BIT) {
    color = color == Color::BLACK ? 1 : 0;
    SELECT(1Bit);
  } else {
    #if INKPLATE_6PLUS | INKPLATE_6PLUS_V2 | INKPLATE_6FLICK
      color = color == Color::BLACK ? 0 : 7;
    #endif

    SELECT(3Bit);
  }

  #undef CODE

  drawArc(pos.x + 10, pos.y + 10, 10, Corner::TOP_LEFT, color);
  drawArc(pos.x + dim.width - 11, pos.y + 10, 10, Corner::TOP_RIGHT, color);
  drawArc(pos.x + 10, pos.y + dim.height - 11, 10, Corner::LOWER_LEFT, color);
  drawArc(pos.x + dim.width - 11, pos.y + dim.height - 11, 10, Corner::LOWER_RIGHT, color);
}

// High performance Colorize a rectangle in the frame buffer
// Does not work with Inkplate-10 as the screen width is not a multiple of 32.

#if !INKPLATE_10

  auto Screen::lowColorize3Bit(Dim dim, Pos pos, uint8_t color) -> void {
    uint32_t *temp =
        ((uint32_t *)(&(frameBuffer3Bit->get_data())[frameBuffer3Bit->get_line_size() * pos.y])) +
        (pos.x >> 3);

    uint32_t color_mask;
    uint16_t line_size_32 = frameBuffer3Bit->get_line_size() >> 2; // 32 bits count
    // int16_t  in_size_32   = (static_cast<int16_t>(dim.width) - (8 - (pos.x & 0x07)) -
    // ((static_cast<int16_t>(dim.width) + pos.x - 1) & 0x07) + 1) / 8;
    int16_t in_size_32 =
        ((dim.width + (pos.x & 0x07) + (8 - ((dim.width + pos.x - 1) & 0x07))) >> 3) - 2;
    uint16_t remaining_line_width_32 = line_size_32 - in_size_32 - 2;

    LOG_D("Line Size: %u, in_size: %d, remaining: %u", line_size_32, in_size_32,
          remaining_line_width_32);
    LOG_D("Pos: [%d, %d], Dim: [%d, %d]", pos.x, pos.y, dim.width, dim.height);

    color_mask = color | (color << 4);
    color_mask |= (color_mask << 8);
    color_mask |= (color_mask << 16);

    uint32_t first_mask = ~(0x77777777 << ((pos.x & 0x07) << 2));
    uint32_t last_mask  = ~(0x77777777 >> ((8 - ((pos.x + dim.width) & 0x07)) << 2));

    LOG_D("Masks: %08" PRIX32 ", %08" PRIX32 ", %08" PRIX32, first_mask, last_mask, color_mask);

    if (in_size_32 < 0) { // All pixels in a row are in the same 32 bits word
      first_mask &= last_mask;
      color_mask &= ~first_mask;
      for (uint16_t i = 0; i < dim.height; i++) {
        *temp = (*temp & first_mask) | color_mask;
        temp += line_size_32;
      }
    } else
      for (uint16_t i = 0; i < dim.height; i++) {
        *temp = (*temp & first_mask) | (color_mask & ~first_mask);
        temp++;
        for (int16_t j = 0; j < in_size_32; j++) {
          *temp++ = color_mask;
        }
        *temp = (*temp & last_mask) | (color_mask & ~last_mask);
        temp += remaining_line_width_32 + 1;
      }
  }

  auto Screen::lowColorize1Bit(Dim dim, Pos pos, uint8_t color) -> void {
    uint32_t *temp =
        ((uint32_t *)(&(frameBuffer1Bit->get_data())[frameBuffer1Bit->get_line_size() * pos.y])) +
        (pos.x >> 5);

    uint16_t line_size_32 = frameBuffer1Bit->get_line_size() >> 2; // 32 bits count
    int16_t in_size_32 =
        ((dim.width + (pos.x & 0x1F) + (32 - ((dim.width + pos.x - 1) & 0x1F))) >> 5) - 2;
    uint16_t remaining_line_width_32 = line_size_32 - in_size_32 - 2;

    LOG_D("Line Size: %u, in_size: %d, remaining: %u", line_size_32, in_size_32,
          remaining_line_width_32);
    LOG_D("Pos: [%d, %d], Dim: [%d, %d]", pos.x, pos.y, dim.width, dim.height);

    if (color == 1) {
      uint32_t first_mask = 0xFFFFFFFF << (pos.x & 0x1F);
      uint32_t last_mask  = 0xFFFFFFFF >> (32 - ((pos.x + dim.width) & 0x1F));

      LOG_D("Masks: %08" PRIX32 ", %08" PRIX32, first_mask, last_mask);

      if (in_size_32 < 0) {
        first_mask &= last_mask;
        for (uint16_t i = 0; i < dim.height; i++) {
          *temp |= first_mask;
          temp += line_size_32;
        }
      } else
        for (uint16_t i = 0; i < dim.height; i++) {
          *temp++ |= first_mask;
          for (int16_t j = 0; j < in_size_32; j++) {
            *temp++ = 0xFFFFFFFF;
          }
          *temp++ |= last_mask;
          temp += remaining_line_width_32;
        }
    } else {
      uint32_t first_mask = ~(0xFFFFFFFF << (pos.x & 0x1F));
      uint32_t last_mask  = ~(0xFFFFFFFF >> (32 - ((pos.x + dim.width) & 0x1F)));

      LOG_D("Masks: %08" PRIX32 ", %08" PRIX32, first_mask, last_mask);

      if (in_size_32 < 0) {
        first_mask |= last_mask;
        for (uint16_t i = 0; i < dim.height; i++) {
          *temp &= first_mask;
          temp += line_size_32;
        }
      } else
        for (uint16_t i = 0; i < dim.height; i++) {
          *temp++ &= first_mask;
          for (int16_t j = 0; j < in_size_32; j++) {
            *temp++ = 0;
          }
          *temp++ &= last_mask;
          temp += remaining_line_width_32;
        }
    }
  }

#endif

auto Screen::colorizeRegion(Dim dim, Pos pos,
                            uint8_t color) -> void //, bool show)
{
  #if !INKPLATE_10
    if (pixelResolution == PixelResolution::ONE_BIT) {
      color = color == Color::BLACK ? 1 : 0;

      switch (orientation) {
      case Orientation::BOTTOM:
        lowColorize1Bit(dim, pos, color);
        break;
      case Orientation::TOP:
        lowColorize1Bit(dim, Pos(width - (pos.x + dim.width), height - (pos.y + dim.height)),
                        color);
        break;
      case Orientation::LEFT:
        lowColorize1Bit(Dim(dim.height, dim.width), Pos(pos.y, width - (pos.x + dim.width)), color);
        break;
      case Orientation::RIGHT:
        lowColorize1Bit(Dim(dim.height, dim.width), Pos(height - (pos.y + dim.height), pos.x),
                        color);
        break;
      }
    } else {
      #if INKPLATE_6PLUS | INKPLATE_6PLUS_V2 | INKPLATE_6FLICK
        color = color == Color::BLACK ? 0 : 7;
      #endif
      switch (orientation) {
      case Orientation::BOTTOM:
        lowColorize3Bit(dim, pos, color);
        break;
      case Orientation::TOP:
        lowColorize3Bit(dim, Pos(width - (pos.x + dim.width), height - (pos.y + dim.height)),
                        color);
        break;
      case Orientation::LEFT:
        lowColorize3Bit(Dim(dim.height, dim.width), Pos(pos.y, width - (pos.x + dim.width)), color);
        break;
      case Orientation::RIGHT:
        lowColorize3Bit(Dim(dim.height, dim.width), Pos(height - (pos.y + dim.height), pos.x),
                        color);
        break;
      }
    }

  #else
    int16_t xMax = pos.x + dim.width;
    int16_t yMax = pos.y + dim.height;

    if (yMax > height) yMax = height;
    if (xMax > width) xMax = width;

    #define CODE(resolution, orientation)                                                          \
      for (int j = pos.y; j < yMax; j++) {                                                         \
        for (int i = pos.x; i < xMax; i++) {                                                       \
          setPixelO##orientation##resolution(i, j, color);                                         \
        }                                                                                          \
      }

    if (pixelResolution == PixelResolution::ONE_BIT) {
      color = color == Color::BLACK ? 1 : 0;
      SELECT(1Bit);
    } else {
      SELECT(3Bit);
    }

    #undef CODE

  #endif
}

auto Screen::drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch) -> void {
  int xMax = pos.x + dim.width;
  int yMax = pos.y + dim.height;

  if (yMax > height) yMax = height;
  if (xMax > width) xMax = width;

  if (pixelResolution == PixelResolution::ONE_BIT) {
    #define CODE(resolution, orientation)                                                          \
      for (uint32_t j = pos.y, q = 0; j < yMax; j++, q++) {                                        \
        for (uint32_t i = pos.x, p = (q * pitch) << 3; i < xMax; i++, p++) {                       \
          uint8_t v = bitmapData[p >> 3] & LUT1BIT[p & 7];                                         \
          if (v) setPixelO##orientation##resolution(i, j, 1);                                      \
        }                                                                                          \
      }
    SELECT(1Bit);
    #undef CODE
  } else {
    #define CODE(resolution, orientation)                                                          \
      for (uint32_t j = pos.y, q = 0; j < yMax; j++, q++) {                                        \
        for (uint32_t i = pos.x, p = q * pitch; i < xMax; i++, p++) {                              \
          uint8_t v = 7 - (bitmapData[p] >> 5);                                                    \
          if (v != 7) setPixelO##orientation##resolution(i, j, v);                                 \
        }                                                                                          \
      }
    SELECT(3Bit);
    #undef CODE
  }
}

auto Screen::setup(PixelResolution resolution, Orientation orientation) -> void {
  setOrientation(orientation);
  setPixelResolution(resolution, true);
  clear();
}

auto Screen::setPixelResolution(PixelResolution resolution, bool force) -> void {
  if (force || (pixelResolution != resolution)) {
    pixelResolution = resolution;
    if (pixelResolution == PixelResolution::ONE_BIT) {
      if (frameBuffer3Bit != nullptr) {
        free(frameBuffer3Bit);
        frameBuffer3Bit = nullptr;
      }
      if ((frameBuffer1Bit = e_ink.new_frame_buffer_1bit()) != nullptr) {
        frameBuffer1Bit->clear();
      }
      partialCount = 0;
    } else {
      if (frameBuffer1Bit != nullptr) {
        free(frameBuffer1Bit);
        frameBuffer1Bit = nullptr;
      }
      if ((frameBuffer3Bit = e_ink.new_frame_buffer_3bit()) != nullptr) {
        frameBuffer3Bit->clear();
      }
    }
  }
}

auto Screen::setOrientation(Orientation orient) -> void {
  orientation = orient;
  if ((orientation == Orientation::LEFT) || (orientation == Orientation::RIGHT)) {
    width  = e_ink.get_height();
    height = e_ink.get_width();
  } else {
    width  = e_ink.get_width();
    height = e_ink.get_height();
  }
}