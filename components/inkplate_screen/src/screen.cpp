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

Screen        Screen::singleton;

uint16_t      Screen::width;
uint16_t      Screen::height;
uint16_t      Screen::yOffset;

const uint8_t Screen::LUT1BIT[8]     = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
const uint8_t Screen::LUT1BIT_INV[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

#define SELECT(resolution)                                                                               \
        if (orientation == Orientation::LEFT) {                                                          \
          CODE(resolution, Left);                                                                        \
        } else if (orientation == Orientation::RIGHT) {                                                  \
          CODE(resolution, Right);                                                                       \
        } else if (orientation == Orientation::BOTTOM) {                                                 \
          CODE(resolution, Bottom);                                                                      \
        } else {                                                                                         \
          CODE(resolution, Top);                                                                         \
        }

#define NIBBLE(k, p) (((k & 1) ? (bitmapData[p] & 0x0F) : (bitmapData[p] >> 4)) << 4)
#define NIBBLE2(k, p) (((k & 1) ? ((bitmapData[p] & 0x0F) >> 1) : (bitmapData[p] >> 5)))

auto Screen::drawPicture(PicturePtr &picture, Pos pos) -> void {

  auto dim        = picture->getDim();
  auto bitmapData = picture->getBitmap();
  auto bpp        = picture->getBitsPerPixel();

  if (pos.x > width) { pos.x = 0; }
  if (pos.y > height) { pos.y = 0; }

  uint32_t xMax = pos.x + dim.width;
  uint32_t yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  if (pixelResolution == PixelResolution::ONE_BIT) {
    static int16_t err[1201]; // This is the maximum width of all Inkplate devices + 1
    int16_t        error;
    memset(err, 0, 1201 * 2);

    if (bpp == 4) {
      uint16_t w = ((dim.width + 1) & 0xFFFE) >> 1; // Round up to even number of pixels
      // LOG_I("Two last nibbles from the first line: {:#04x} {:#04x}", NIBBLE(0, w-1), NIBBLE(1, w-1));
      #define CODE(resolution, orientation)                                                                    \
              for (int j = pos.y, q = 0; j < yMax; j++, q++) {                                                 \
                for (int i = pos.x, p = q * w, k = 0; i < (xMax - 1); i++, k++) {                              \
                  int32_t v = NIBBLE(k, p) + err[k + 1];                                                       \
                  if (v > 128) {                                                                               \
                    error = (v - 255);                                                                         \
                    setPixelO ## orientation ## resolution(i, j, 0);                                           \
                  } else {                                                                                     \
                    error = v;                                                                                 \
                    setPixelO ## orientation ## resolution(i, j, 1);                                           \
                  }                                                                                            \
                  if (k != 0) {                                                                                \
                    err[k - 1] += error / 8;                                                                   \
                  }                                                                                            \
                  err[k] += 3 * error / 8;                                                                     \
                  err[k + 1] = error / 8;                                                                      \
                  err[k + 2] += 3 * error / 8;                                                                 \
                  if (k & 1) { ++p; }                                                                          \
                }                                                                                              \
              }

      SELECT(1Bit);
      #undef CODE
    }
    else {
      #define CODE(resolution, orientation)                                                                    \
              for (int j = pos.y, q = 0; j < yMax; j++, q++) {                                                 \
                for (int i = pos.x, p = q * dim.width, k = 0; i < (xMax - 1); i++, p++, k++) {                 \
                  int32_t v = bitmapData[p] + err[k + 1];                                                      \
                  if (v > 128) {                                                                               \
                    error = (v - 255);                                                                         \
                    setPixelO ## orientation ## resolution(i, j, 0);                                           \
                  } else {                                                                                     \
                    error = v;                                                                                 \
                    setPixelO ## orientation ## resolution(i, j, 1);                                           \
                  }                                                                                            \
                  if (k != 0) {                                                                                \
                    err[k - 1] += error / 8;                                                                   \
                  }                                                                                            \
                  err[k] += 3 * error / 8;                                                                     \
                  err[k + 1] = error / 8;                                                                      \
                  err[k + 2] += 3 * error / 8;                                                                 \
                }                                                                                              \
              }

      SELECT(1Bit);
      #undef CODE
    }
  } else {
    if (bpp == 4) {
      uint16_t w = ((dim.width + 1) & 0xFFFE) >> 1; // Round up to even number of pixels
      #define CODE(resolution, orientation)                                                                    \
              for (int j = pos.y, q = 0; j < yMax; j++, q++) {                                                 \
                for (int i = pos.x, p = q * w, k = 0; i < xMax; i++, k++) {                                    \
                  int8_t v = NIBBLE2(k, p);                                                                    \
                  setPixelO ## orientation ## resolution(i, j, v);                                             \
                  if (k & 1) { ++p; }                                                                          \
                }                                                                                              \
              }

      SELECT(3Bit);
      #undef CODE
    } else {
      #define CODE(resolution, orientation)                                                                    \
              for (int j = pos.y, q = 0; j < yMax; j++, q++) {                                                 \
                for (int i = pos.x, p = q * dim.width; i < xMax; i++, p++) {                                   \
                  int8_t v = bitmapData[p] >> 5;                                                               \
                  setPixelO ## orientation ## resolution(i, j, v);                                             \
                }                                                                                              \
              }
      SELECT(3Bit);
      #undef CODE
    }
  }
}

#undef NIBBLE
#undef NIBBLE2

auto Screen::drawRectangle(Dim dim, Pos pos, uint8_t color) -> void {
  uint32_t xMax = pos.x + dim.width;
  uint32_t yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  #define CODE(resolution, orientation)                                                                    \
          for (int i = pos.x; i < xMax; i++) {                                                             \
            setPixelO ## orientation ## resolution(i, pos.y, color);                                       \
            setPixelO ## orientation ## resolution(i, yMax - 1, color);                                    \
          }                                                                                                \
          for (int j = pos.y; j < yMax; j++) {                                                             \
            setPixelO ## orientation ## resolution(pos.x, j, color);                                       \
            setPixelO ## orientation ## resolution(xMax - 1, j, color);                                    \
          }

  if (pixelResolution == PixelResolution::ONE_BIT) {
    color = color == Color::BLACK ? 1 : 0;
    SELECT(1Bit);
  } else {
    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || INKPLATE_10_V2
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

  #define CODE(resolution, orientation)                                                              \
          while (x < y) {                                                                                  \
            if (f >= 0) {                                                                                  \
              y--;                                                                                         \
              ddF_y += 2;                                                                                  \
              f += ddF_y;                                                                                  \
            }                                                                                              \
            x++;                                                                                           \
            ddF_x += 2;                                                                                    \
            f += ddF_x;                                                                                    \
                                                                                                   \
            switch (corner) {                                                                              \
            case Corner::TOP_LEFT:                                                                         \
              setPixelO ## orientation ## resolution(xMid - x, yMid - y, color);                               \
              setPixelO ## orientation ## resolution(xMid - y, yMid - x, color);                               \
              break;                                                                                       \
                                                                                                   \
            case Corner::TOP_RIGHT:                                                                        \
              setPixelO ## orientation ## resolution(xMid + x, yMid - y, color);                               \
              setPixelO ## orientation ## resolution(xMid + y, yMid - x, color);                               \
              break;                                                                                       \
                                                                                                   \
            case Corner::LOWER_LEFT:                                                                       \
              setPixelO ## orientation ## resolution(xMid - x, yMid + y, color);                               \
              setPixelO ## orientation ## resolution(xMid - y, yMid + x, color);                               \
              break;                                                                                       \
                                                                                                   \
            case Corner::LOWER_RIGHT:                                                                      \
              setPixelO ## orientation ## resolution(xMid + x, yMid + y, color);                               \
              setPixelO ## orientation ## resolution(xMid + y, yMid + x, color);                               \
              break;                                                                                       \
            }                                                                                              \
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

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  #define CODE(resolution, orientation)                                                              \
          for (int i = pos.x + 10; i < xMax - 10; i++) {                                                   \
            setPixelO ## orientation ## resolution(i, pos.y, color);                                           \
            setPixelO ## orientation ## resolution(i, yMax - 1, color);                                        \
          }                                                                                                \
          for (int j = pos.y + 10; j < yMax - 10; j++) {                                                   \
            setPixelO ## orientation ## resolution(pos.x, j, color);                                           \
            setPixelO ## orientation ## resolution(xMax - 1, j, color);                                        \
          }

  if (pixelResolution == PixelResolution::ONE_BIT) {
    color = color == Color::BLACK ? 1 : 0;
    SELECT(1Bit);
  } else {
    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || INKPLATE_10_V2
      color = color == Color::BLACK ? 0 : 7;
    #endif

    SELECT(3Bit);
  }

  #undef CODE

  drawArc(pos.x + 10,             pos.y + 10,              10, Corner::TOP_LEFT,    color);
  drawArc(pos.x + dim.width - 11, pos.y + 10,              10, Corner::TOP_RIGHT,   color);
  drawArc(pos.x + 10,             pos.y + dim.height - 11, 10, Corner::LOWER_LEFT,  color);
  drawArc(pos.x + dim.width - 11, pos.y + dim.height - 11, 10, Corner::LOWER_RIGHT, color);
}

// High performance Colorize a rectangle in the frame buffer
// Does not work with Inkplate-10 and 10_V2 as the screen width is not a multiple of 32.

#if !(INKPLATE_10 || INKPLATE_10_V2)

  auto Screen::lowColorize3Bit(Dim dim, Pos pos, uint8_t color) -> void {
    uint32_t *temp =
      ((uint32_t *)(&(frameBufferGrayScale->getData())[frameBufferGrayScale->getLineSize() * pos.y])) +
      (pos.x >> 3);

    uint32_t colorMask;
    uint16_t lineSize32 = frameBufferGrayScale->getLineSize() >> 2; // 32 bits count
    // int16_t  inSize32   = (static_cast<int16_t>(dim.width) - (8 - (pos.x & 0x07)) -
    // ((static_cast<int16_t>(dim.width) + pos.x - 1) & 0x07) + 1) / 8;
    int16_t inSize32 =
      ((dim.width + (pos.x & 0x07) + (8 - ((dim.width + pos.x - 1) & 0x07))) >> 3) - 2;
    uint16_t remainingLineWidth32 = lineSize32 - inSize32 - 2;

    LOG_D("Line Size: {}, inSize: {}, remaining: {}", lineSize32, inSize32,
          remainingLineWidth32);
    LOG_D("Pos: [{}, {}], Dim: [{}, {}]", pos.x, pos.y, dim.width, dim.height);

    colorMask = color | (color << 4);
    colorMask |= (colorMask << 8);
    colorMask |= (colorMask << 16);

    uint32_t firstMask = ~(0x77777777 << ((pos.x & 0x07) << 2));
    uint32_t lastMask  = ~(0x77777777 >> ((8 - ((pos.x + dim.width) & 0x07)) << 2));

    LOG_D("Masks: {:#010x}, {:010x}, {:010x}", firstMask, lastMask, colorMask);

    if (inSize32 < 0) { // All pixels in a row are in the same 32 bits word
      firstMask &= lastMask;
      colorMask &= ~firstMask;
      for (uint16_t i = 0; i < dim.height; i++) {
        *temp = (*temp & firstMask) | colorMask;
        temp += lineSize32;
      }
    } else {
      for (uint16_t i = 0; i < dim.height; i++) {
        *temp = (*temp & firstMask) | (colorMask & ~firstMask);
        temp++;
        for (int16_t j = 0; j < inSize32; j++) {
          *temp++ = colorMask;
        }
        *temp = (*temp & lastMask) | (colorMask & ~lastMask);
        temp += remainingLineWidth32 + 1;
      }
    }
  }

  auto Screen::lowColorize1Bit(Dim dim, Pos pos, uint8_t color) -> void {
    uint32_t *temp =
      ((uint32_t *)(&(frameBufferMono->getData())[frameBufferMono->getLineSize() * pos.y])) +
      (pos.x >> 5);

    uint16_t lineSize32 = frameBufferMono->getLineSize() >> 2; // 32 bits count
    int16_t  inSize32 =
      ((dim.width + (pos.x & 0x1F) + (32 - ((dim.width + pos.x - 1) & 0x1F))) >> 5) - 2;
    uint16_t remainingLineWidth32 = lineSize32 - inSize32 - 2;

    LOG_D("Line Size: {}, inSize: {}, remaining: {}", lineSize32, inSize32,
          remainingLineWidth32);
    LOG_D("Pos: [{}, {}], Dim: [{}, {}]", pos.x, pos.y, dim.width, dim.height);

    if (color == COLOR) {
      uint32_t firstMask = 0xFFFFFFFF << (pos.x & 0x1F);
      uint32_t lastMask  = 0xFFFFFFFF >> (32 - ((pos.x + dim.width) & 0x1F));

      LOG_D("Masks: {:#010x}, {:#010x}", firstMask, lastMask);

      if (inSize32 < 0) {
        firstMask &= lastMask;
        for (uint16_t i = 0; i < dim.height; i++) {
          *temp |= firstMask;
          temp += lineSize32;
        }
      } else {
        for (uint16_t i = 0; i < dim.height; i++) {
          *temp++ |= firstMask;
          for (int16_t j = 0; j < inSize32; j++) {
            *temp++ = 0xFFFFFFFF;
          }
          *temp++ |= lastMask;
          temp += remainingLineWidth32;
        }
      }
    } else {
      uint32_t firstMask = ~(0xFFFFFFFF << (pos.x & 0x1F));
      uint32_t lastMask  = ~(0xFFFFFFFF >> (32 - ((pos.x + dim.width) & 0x1F)));

      LOG_D("Masks: {:#010x}, {:#010x}", firstMask, lastMask);

      if (inSize32 < 0) {
        firstMask |= lastMask;
        for (uint16_t i = 0; i < dim.height; i++) {
          *temp &= firstMask;
          temp += lineSize32;
        }
      } else {
        for (uint16_t i = 0; i < dim.height; i++) {
          *temp++ &= firstMask;
          for (int16_t j = 0; j < inSize32; j++) {
            *temp++ = 0;
          }
          *temp++ &= lastMask;
          temp += remainingLineWidth32;
        }
      }
    }
  }

#endif

auto Screen::colorizeRegion(Dim dim, Pos pos,
                            uint8_t color) -> void //, bool show)
{
  #if !(INKPLATE_10 || INKPLATE_10_V2)
    if (pixelResolution == PixelResolution::ONE_BIT) {
      color = color == Color::BLACK ? 1 : 0;

      switch (orientation) {
      case Orientation::BOTTOM:
        lowColorize1Bit(dim, pos, color);
        break;
      case Orientation::TOP:
        lowColorize1Bit(dim, Pos(width - (pos.x + dim.width), height - (pos.y + dim.height)), color);
        break;
      case Orientation::LEFT:
        lowColorize1Bit(Dim(dim.height, dim.width), Pos(pos.y, width - (pos.x + dim.width)), color);
        break;
      case Orientation::RIGHT:
        lowColorize1Bit(Dim(dim.height, dim.width), Pos(height - (pos.y + dim.height), pos.x), color);
        break;
      }
    } else {
      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
        color = color == Color::BLACK ? 0 : 7;
      #endif
      switch (orientation) {
      case Orientation::BOTTOM:
        lowColorize3Bit(dim, pos, color);
        break;
      case Orientation::TOP:
        lowColorize3Bit(dim, Pos(width - (pos.x + dim.width), height - (pos.y + dim.height)), color);
        break;
      case Orientation::LEFT:
        lowColorize3Bit(Dim(dim.height, dim.width), Pos(pos.y, width - (pos.x + dim.width)), color);
        break;
      case Orientation::RIGHT:
        lowColorize3Bit(Dim(dim.height, dim.width), Pos(height - (pos.y + dim.height), pos.x), color);
        break;
      }
    }

  #else
    int16_t xMax = pos.x + dim.width;
    int16_t yMax = pos.y + dim.height;

    if (yMax > height) { yMax = height; }
    if (xMax > width) { xMax = width; }

    #define CODE(resolution, orientation)                                                            \
            for (int j = pos.y; j < yMax; j++) {                                                           \
              for (int i = pos.x; i < xMax; i++) {                                                         \
                setPixelO ## orientation ## resolution(i, j, color);                                           \
              }                                                                                            \
            }

    if (pixelResolution == PixelResolution::ONE_BIT) {
      color = color == Color::BLACK ? 1 : 0;
      SELECT(1Bit);
    } else {
      color = color == Color::BLACK ? 0 : 7;
      SELECT(3Bit);
    }

    #undef CODE

  #endif
}

auto Screen::drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch) -> void {
  int xMax = pos.x + dim.width;
  int yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  if (pixelResolution == PixelResolution::ONE_BIT) {
    #define CODE(resolution, orientation)                                                              \
            for (uint32_t j = pos.y, q = 0; j < yMax; j++, q++) {                                            \
              for (uint32_t i = pos.x, p = (q * pitch) << 3; i < xMax; i++, p++) {                           \
                uint8_t v = bitmapData[p >> 3] &LUT1BIT[p & 7];                                             \
                if (v) setPixelO ## orientation ## resolution(i, j, 1);                                          \
              }                                                                                              \
            }
    SELECT(1Bit);
    #undef CODE
  } else {
    #define CODE(resolution, orientation)                                                              \
            for (uint32_t j = pos.y, q = 0; j < yMax; j++, q++) {                                            \
              for (uint32_t i = pos.x, p = q * pitch; i < xMax; i++, p++) {                                  \
                uint8_t v = 7 - (bitmapData[p] >> 5);                                                        \
                if (v != 7) setPixelO ## orientation ## resolution(i, j, v);                                     \
              }                                                                                              \
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

      if (frameBufferGrayScale != nullptr) {
        free(frameBufferGrayScale);
        frameBufferGrayScale = nullptr;
      }

      if ((frameBufferMono = platform.getEInk().newFrameBufferMono()) != nullptr) {
        frameBufferMono->clear();
      }
      partialCount = 0;
    } else {
      if (frameBufferMono != nullptr) {
        free(frameBufferMono);
        frameBufferMono = nullptr;
      }

      if ((frameBufferGrayScale = platform.getEInk().newFrameBufferGrayScale()) != nullptr) {
        frameBufferGrayScale->clear();
      }
    }
  }

  setYOffset();
}

auto Screen::setOrientation(Orientation orient) -> void {
  orientation = orient;

  if ((orientation == Orientation::LEFT) || (orientation == Orientation::RIGHT)) {
    width  = platform.getEInk().getHeight();
    height = platform.getEInk().getWidth();
  } else {
    width  = platform.getEInk().getWidth();
    height = platform.getEInk().getHeight();
  }

  setYOffset();
}

auto Screen::setYOffset() -> void {
  yOffset = 0;

  if (orientation == Orientation::TOP) {
    if (pixelResolution == PixelResolution::ONE_BIT) {
      yOffset = ((height + 7) & 0xFFF8) - height;
    } else {
      yOffset = ((height + 1) & 0xFFFE) - height;
    }
  }

  LOG_I("Setting yOffset to {}", yOffset);
}

auto Screen::setPixelOLeft1Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferMono->getData())[frameBufferMono->getDataSize() -
      (frameBufferMono->getLineSize() * (col + 1)) + (row >> 3)];
  #if PIXELS_BIG_ENDIAN
    if (color == COLOR) {
      *temp = *temp | LUT1BIT[row & 7];
    }
    else {
      *temp = (*temp & ~LUT1BIT[row & 7]);
    }
  #else
    if (color == COLOR) {
      *temp = *temp | LUT1BIT_INV[row & 7];
    }
    else {
      *temp = (*temp & ~LUT1BIT_INV[row & 7]);
    }
  #endif
}

auto Screen::setPixelORight1Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferMono
      ->getData())[(frameBufferMono->getLineSize() * (col + 1)) - (row >> 3) - 1];

  #if PIXELS_BIG_ENDIAN
    if (color == COLOR) {
      *temp = (*temp | LUT1BIT_INV[row & 7]);
    }
    else {
      *temp = (*temp & ~LUT1BIT_INV[row & 7]);
    }
  #else
    if (color == COLOR) {
      *temp = *temp | LUT1BIT[row & 7];
    }
    else {
      *temp = (*temp & ~LUT1BIT[row & 7]);
    }
  #endif
}

auto Screen::setPixelOBottom1Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferMono->getData())[frameBufferMono->getLineSize() * row + (col >> 3)];

  #if PIXELS_BIG_ENDIAN
    if (color == COLOR) {
      *temp = *temp | LUT1BIT[col & 7];
    }
    else {
      *temp = (*temp & ~LUT1BIT[col & 7]);
    }
  #else
    if (color == COLOR) {
      *temp = *temp | LUT1BIT_INV[col & 7];
    }
    else {
      *temp = (*temp & ~LUT1BIT_INV[col & 7]);
    }
  #endif
}

auto Screen::setPixelOTop1Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
  uint8_t *temp = &(frameBufferMono->getData())[frameBufferMono->getDataSize() -
                    (frameBufferMono->getLineSize() * row) - ((col + yOffset) >> 3)];

  #if PIXELS_BIG_ENDIAN
    if (color == COLOR) {
      *temp = *temp | LUT1BIT_INV[((col + yOffset) & 7)];
    }
    else {
      *temp = (*temp & ~LUT1BIT_INV[((col + yOffset) & 7)]);
    }
  #else
    if (color == COLOR) {
      *temp = *temp | LUT1BIT[(col + yOffset) & 7];
    }
    else {
      *temp = (*temp & ~LUT1BIT[(col + yOffset) & 7]);
    }
  #endif
}

auto Screen::setPixelOLeft3Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferGrayScale->getData())[frameBufferGrayScale->getDataSize() -
                                    (frameBufferGrayScale->getLineSize() * (col + 1)) + (row >> 1)];
  if (row & 1) {
    *temp = (*temp & 0xF0) | color;
  }
  else {
    *temp = (*temp & 0x0F) | (color << 4);
  }
}

auto Screen::setPixelORight3Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferGrayScale
      ->getData())[(frameBufferGrayScale->getLineSize() * (col + 1)) - (row >> 1) - 1];
  if (row & 1) {
    *temp = (*temp & 0x0F) | (color << 4);
  }
  else {
    *temp = (*temp & 0xF0) | color;
  }
}

auto Screen::setPixelOBottom3Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferGrayScale->getData())[frameBufferGrayScale->getLineSize() * row + (col >> 1)];
  if (col & 1) {
    *temp = (*temp & 0xF0) | color;
  }
  else {
    *temp = (*temp & 0x0F) | (color << 4);
  }
}

auto Screen::setPixelOTop3Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferGrayScale->getData())[frameBufferGrayScale->getDataSize() -
                                    (frameBufferGrayScale->getLineSize() * row) - ((col + yOffset) >> 1)];
  if ((col + yOffset) & 1) {
    *temp = (*temp & 0x0F) | (color << 4);
  }
  else {
    *temp = (*temp & 0xF0) | color;
  }
}