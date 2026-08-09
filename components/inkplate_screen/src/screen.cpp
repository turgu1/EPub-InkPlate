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

Screen        Screen::singleton;

uint16_t      Screen::width;
uint16_t      Screen::height;
uint16_t      Screen::yOffset;

const uint8_t Screen::LUT_MONO[8]     = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
const uint8_t Screen::LUT_MONO_INV[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

#define NIBBLE(k, p) (((k & 1) ? (bitmapData[p] & 0x0F) : (bitmapData[p] >> 4)) << 4)
#if COLOR_INVERTED
  #if GRAY_3_BITS
    #define NIBBLE2(k, p) (7 - (((k & 1) ? ((bitmapData[p] & 0x0F) >> 1) : (bitmapData[p] >> 5))))
  #else
    #define NIBBLE2(k, p) (15 - (((k & 1) ? (bitmapData[p] & 0x0F) : (bitmapData[p] >> 4))))
  #endif
#else
  #if GRAY_3_BITS
    #define NIBBLE2(k, p) (((k & 1) ? ((bitmapData[p] & 0x0F) >> 1) : (bitmapData[p] >> 5)))
  #else
    #define NIBBLE2(k, p) (((k & 1) ? (bitmapData[p] & 0x0F) : (bitmapData[p] >> 4)))
  #endif
#endif

auto Screen::drawPicture(PicturePtr &picture, Pos pos) -> void {

  auto dim        = picture->getDim();
  auto bitmapData = picture->getBitmap();
  auto bpp        = picture->getBitsPerPixel();

  if (pos.x > width) { pos.x = 0; }
  if (pos.y > height) { pos.y = 0; }

  uint16_t xMax = pos.x + dim.width;
  uint16_t yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  if (pixelResolution == PixelResolution::MONO) {
    static int16_t err[1201]; // This is the maximum width of all Inkplate devices + 1
    int16_t        error;
    memset(err, 0, 1201 * 2);

    auto black = platform.getEInk().getMonoBlack();
    auto white = platform.getEInk().getMonoWhite();

    if (bpp == 4) {
      uint16_t w = ((dim.width + 1) & 0xFFFE) >> 1; // Round up to even number of pixels
      // LOG_I("Two last nibbles from the first line: {:#04x} {:#04x}", NIBBLE(0, w-1), NIBBLE(1, w-1));
      for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
        uint32_t p = q * w;
        for (uint16_t i = pos.x, k = 0; i < (xMax - 1); ++i, ++k) {
          int32_t v = NIBBLE(k, p) + err[k + 1];
          if (v > 128) {
            error = (v - 255);
            setPixel({i, j}, white);
          } else {
            error = v;
            setPixel({i, j}, black);
          }
          if (k != 0) {
            err[k - 1] += error >> 3;
          }
          err[k] += 3 * error >> 3;
          err[k + 1] = error >> 3;
          err[k + 2] += 3 * error >> 3;
          if (k & 1) { ++p; }
        }
      }
    }
    else {
      for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
        uint32_t p = q * dim.width;
        for (uint16_t i = pos.x, k = 0; i < (xMax - 1); ++i, ++p, ++k) {
          int32_t v = bitmapData[p] + err[k + 1];
          if (v > 128) {
            error = (v - 255);
            setPixel({i, j}, white);
          } else {
            error = v;
            setPixel({i, j}, black);
          }
          if (k != 0) {
            err[k - 1] += error >> 3;
          }
          err[k] += 3 * error >> 3;
          err[k + 1] = error >> 3;
          err[k + 2] += 3 * error >> 3;
        }
      }
    }
  } else {
    if (bpp == 4) {
      uint16_t w = ((dim.width + 1) & 0xFFFE) >> 1; // Round up to even number of pixels
      for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
        uint32_t p = q * w;
        for (uint16_t i = pos.x, k = 0; i < xMax; ++i, ++k) {
          int8_t v = NIBBLE2(k, p);
          setPixel({i, j}, v);
          if (k & 1) { ++p; }
        }
      }
    } else {
      for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
        uint32_t p = q * dim.width;
        for (uint16_t i = pos.x; i < xMax; ++i, ++p) {
          #if GRAY_4_BITS
            #if COLOR_INVERTED
              int8_t v = 15 - (bitmapData[p] >> 4);
            #else
              int8_t v = bitmapData[p] >> 4;
            #endif
          #else
            #if COLOR_INVERTED
              int8_t v = 7 - (bitmapData[p] >> 5);
            #else
              int8_t v = bitmapData[p] >> 5;
            #endif
          #endif
          setPixel({i, j}, v);
        }
      }
    }
  }
}

#undef NIBBLE
#undef NIBBLE2

auto Screen::drawRectangle(Dim dim, Pos pos, uint8_t color, int8_t thickness) -> void {
  uint16_t xMax = pos.x + dim.width;
  uint16_t yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  if (pixelResolution == PixelResolution::MONO) {
    color = (color == Color::BLACK) ? platform.getEInk().getMonoBlack() : platform.getEInk().getMonoWhite();
  } else {
    color = (color == Color::BLACK) ? platform.getEInk().getGrayBlack() : platform.getEInk().getGrayWhite();
  }

  for (uint16_t i = pos.x; i < xMax; ++i) {
    setPixel({i, pos.y}, color);
    setPixel({i, (uint16_t)(yMax - 1)}, color);
  }
  for (uint16_t j = pos.y; j < yMax; ++j) {
    setPixel({pos.x, j}, color);
    setPixel({(uint16_t)(xMax - 1), j}, color);
  }
}

/**
 * Draws a solid horizontal row using single pixel iterations.
 */
auto Screen::drawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint8_t color) -> void {
  for (uint16_t x = x0; x <= x1; ++x) {
    setPixel({ x, y }, color);
  }
}

/**
 * Draws a rounded rectangle into any arbitrary bitmap format via functional abstraction.
 * 
 * @param dim        The rectangle dimension
 * @param p0         Top-left position coordinates
 * @param color      Color payload value (0/1 for 1-bpp, 0-15 for 4-bpp)
 * @param radius     Corner rounding radius
 * @param thickness  Line thickness growing inward (-1 for solid, -2 for 1/2 dither, -3 for 1/3 dither)
 */
auto Screen::drawRoundedRectangle(Dim dim, Pos p0, uint8_t color, int8_t thickness, int8_t radius) -> void {
  if (dim.width == 0 || dim.height == 0) return;

  // Compute inclusive outer boundaries (p1 is the last drawable pixel index)
  Pos p1 = { uint16_t(p0.x + dim.width - 1), uint16_t(p0.y + dim.height - 1) };

  if (pixelResolution == PixelResolution::MONO) {
    color = (color == Color::BLACK) ? platform.getEInk().getMonoBlack() : platform.getEInk().getMonoWhite();
  } else {
    color = (color == Color::BLACK) ? platform.getEInk().getGrayBlack() : platform.getEInk().getGrayWhite();
  }

  // Validate and clamp radius
  if (2 * radius > (p1.x - p0.x)) radius = (p1.x - p0.x) / 2;
  if (2 * radius > (p1.y - p0.y)) radius = (p1.y - p0.y) / 2;
  if (radius < 0) radius = 0;

  // Cleanly separate mode selection to avoid implicit signed/unsigned comparison bugs
  bool fill = false;
  int8_t spacing = 1;

  if (thickness < 0) {
    fill = true;
    spacing = -thickness; // -2 becomes spacing 2, -3 becomes spacing 3
  } else {
    // Safely promote to signed integers before comparison
    int32_t req_thickness = thickness * 2;
    int32_t max_width = p1.x - p0.x + 1;
    int32_t max_height = p1.y - p0.y + 1;
    if (req_thickness >= max_width || req_thickness >= max_height) {
      fill = true;
    }
  }

  if (fill) {
    // Inline helper lambda to safely intercept and grid-dither the lines
    auto drawFillLine = [this, spacing, color](uint16_t x0, uint16_t x1, uint16_t y) {
      if (spacing > 1) {
        for (uint16_t x = x0; x <= x1; ++x) {
          // Staggered pattern: (x + y) ensures an elegant diagonal gray mesh
          if ((x + y) % spacing == 0) {
            setPixel({ x, y }, color);
          }
        }
      } else {
        drawHLine(x0, x1, y, color);
      }
    };

    // --- FILL MODE ---
    uint16_t cx_left  = p0.x + radius;
    uint16_t cx_right = p1.x - radius;
    uint16_t cy_top   = p0.y + radius;
    uint16_t cy_bot   = p1.y - radius;

    // 1. Fill top and bottom rounded canopies simultaneously
    if (radius > 0) {
      int16_t cx = 0;
      int16_t cy = radius;
      int16_t d  = 3 - 2 * radius;

      while (cx <= cy) {
        // Draw rows corresponding to vertical steps
        drawFillLine(cx_left - cx, cx_right + cx, cy_top - cy);
        drawFillLine(cx_left - cx, cx_right + cx, cy_bot + cy);

        // Draw rows corresponding to horizontal steps
        drawFillLine(cx_left - cy, cx_right + cy, cy_top - cx);
        drawFillLine(cx_left - cy, cx_right + cy, cy_bot + cx);

        if (d < 0) {
          d += 4 * cx + 6;
        } else {
          d += 4 * (cx - cy) + 10;
          cy--;
        }
        cx++;
      }
    }

    // 2. Fill massive center body square
    uint16_t y_start = p0.y + radius;
    uint16_t y_end   = p1.y - radius;
    for (uint16_t y = y_start; y <= y_end; y++) {
      drawFillLine(p0.x, p1.x, y);
    }

  } else {
    // --- THICK OUTLINE MODE ---
    for (uint8_t i = 0; i < thickness; i++) {
      if ((p0.x + i) > (p1.x - i) || (p0.y + i) > (p1.y - i)) break;

      uint16_t cur_x0     = p0.x + i;
      uint16_t cur_y0     = p0.y + i;
      uint16_t cur_x1     = p1.x - i;
      uint16_t cur_y1     = p1.y - i;
      int16_t  cur_radius = radius - i; 

      if (cur_radius < 0) cur_radius = 0;

      // Draw horizontal borders
      drawHLine(cur_x0 + cur_radius, cur_x1 - cur_radius, cur_y0, color);
      drawHLine(cur_x0 + cur_radius, cur_x1 - cur_radius, cur_y1, color);

      // Draw vertical borders
      for (uint16_t y = cur_y0 + cur_radius; y <= cur_y1 - cur_radius; ++y) {
        setPixel({ cur_x0, y }, color);
        setPixel({ cur_x1, y }, color);
      }

      // Draw corner arc accents
      if (cur_radius > 0) {
        uint16_t cx_left  = cur_x0 + cur_radius;
        uint16_t cx_right = cur_x1 - cur_radius;
        uint16_t cy_top   = cur_y0 + cur_radius;
        uint16_t cy_bot   = cur_y1 - cur_radius;
        
        int16_t cx = 0;
        int16_t cy = cur_radius;
        int16_t d  = 3 - 2 * cur_radius; 

        while (cx <= cy) {
          setPixel({ uint16_t(cx_left  - cy), uint16_t(cy_top - cx) }, color);
          setPixel({ uint16_t(cx_left  - cx), uint16_t(cy_top - cy) }, color);
          setPixel({ uint16_t(cx_right + cx), uint16_t(cy_top - cy) }, color);
          setPixel({ uint16_t(cx_right + cy), uint16_t(cy_top - cx) }, color);
          setPixel({ uint16_t(cx_left  - cy), uint16_t(cy_bot + cx) }, color);
          setPixel({ uint16_t(cx_left  - cx), uint16_t(cy_bot + cy) }, color);
          setPixel({ uint16_t(cx_right + cx), uint16_t(cy_bot + cy) }, color);
          setPixel({ uint16_t(cx_right + cy), uint16_t(cy_bot + cx) }, color);

          if (d < 0) { 
            d += 4 * cx + 6; 
          } else { 
            d += 4 * (cx - cy) + 10; 
            cy--; 
          }
          ++cx;
        }
      }
    }
  }
}

// /**
//  * Draws a solid horizontal row using single pixel iterations.
//  */
// auto Screen::drawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint8_t color) -> void {
//   for (uint16_t x = x0; x <= x1; ++x) {
//     setPixel({ x, y }, color);
//   }
// }

// /**
//  * Draws a rounded rectangle into any arbitrary bitmap format via functional abstraction.
//  * 
//  * @param dim        The rectangle dimension
//  * @param p0         Top-left position coordinates
//  * @param color      Color payload value (0/1 for 1-bpp, 0-15 for 4-bpp)
//  * @param radius     Corner rounding radius
//  * @param thickness  Line thickness growing inward (-1 for solid fill)
//  */
// auto Screen::drawRoundedRectangle(Dim dim, Pos p0, uint8_t color, int8_t thickness, int8_t radius) -> void {
//   if (dim.width == 0 || dim.height == 0) return;

//   // Compute inclusive outer boundaries (p1 is the last drawable pixel index)
//   Pos p1 = { uint16_t(p0.x + dim.width - 1), uint16_t(p0.y + dim.height - 1) };

//   if (pixelResolution == PixelResolution::MONO) {
//     color = (color == Color::BLACK) ? platform.getEInk().getMonoBlack() : platform.getEInk().getMonoWhite();
//   } else {
//     color = (color == Color::BLACK) ? platform.getEInk().getGrayBlack() : platform.getEInk().getGrayWhite();
//   }

//   // Validate and clamp radius
//   if (2 * radius > (p1.x - p0.x)) radius = (p1.x - p0.x) / 2;
//   if (2 * radius > (p1.y - p0.y)) radius = (p1.y - p0.y) / 2;
//   if (radius < 0) radius = 0;

//   // Detect if drawing in solid fill mode
//   bool fill = (thickness == -1 || (thickness * 2) >= (p1.x - p0.x + 1) || (thickness * 2) >= (p1.y - p0.y + 1));

//   if (fill) {
//     // --- SOLID FILL MODE ---
//     uint16_t cx_left  = p0.x + radius;
//     uint16_t cx_right = p1.x - radius;
//     uint16_t cy_top   = p0.y + radius;
//     uint16_t cy_bot   = p1.y - radius;

//     // 1. Fill top and bottom rounded canopies simultaneously
//     if (radius > 0) {
//       int16_t cx = 0;
//       int16_t cy = radius;
//       int16_t d  = 3 - 2 * radius; // Signed decision variable

//       while (cx <= cy) {
//         // Draw rows corresponding to vertical steps
//         drawHLine(cx_left - cx, cx_right + cx, cy_top - cy, color);
//         drawHLine(cx_left - cx, cx_right + cx, cy_bot + cy, color);

//         // Draw rows corresponding to horizontal steps
//         drawHLine(cx_left - cy, cx_right + cy, cy_top - cx, color);
//         drawHLine(cx_left - cy, cx_right + cy, cy_bot + cx, color);

//         if (d < 0) {
//           d += 4 * cx + 6;
//         } else {
//           d += 4 * (cx - cy) + 10;
//           cy--;
//         }
//         cx++;
//       }
//     }

//     // 2. Fill massive center body square
//     uint16_t y_start = p0.y + radius;
//     uint16_t y_end   = p1.y - radius;
//     for (uint16_t y = y_start; y <= y_end; y++) {
//       drawHLine(p0.x, p1.x, y, color);
//     }

//   } else {
//     // --- THICK OUTLINE MODE ---
//     for (uint8_t i = 0; i < thickness; i++) {
//       // Gracefully prevent thin shapes from folding inward over themselves
//       if ((p0.x + i) > (p1.x - i) || (p0.y + i) > (p1.y - i)) break;

//       uint16_t cur_x0     = p0.x + i;
//       uint16_t cur_y0     = p0.y + i;
//       uint16_t cur_x1     = p1.x - i;
//       uint16_t cur_y1     = p1.y - i;
//       int16_t  cur_radius = radius - i; 

//       if (cur_radius < 0) cur_radius = 0;

//       // Draw horizontal borders
//       drawHLine(cur_x0 + cur_radius, cur_x1 - cur_radius, cur_y0, color);
//       drawHLine(cur_x0 + cur_radius, cur_x1 - cur_radius, cur_y1, color);

//       // Draw vertical borders
//       for (uint16_t y = cur_y0 + cur_radius; y <= cur_y1 - cur_radius; ++y) {
//         setPixel({ cur_x0, y }, color);
//         setPixel({ cur_x1, y }, color);
//       }

//       // Draw corner arc accents
//       if (cur_radius > 0) {
//         uint16_t cx_left  = cur_x0 + cur_radius;
//         uint16_t cx_right = cur_x1 - cur_radius;
//         uint16_t cy_top   = cur_y0 + cur_radius;
//         uint16_t cy_bot   = cur_y1 - cur_radius;
        
//         int16_t cx = 0;
//         int16_t cy = cur_radius;
//         int16_t d  = 3 - 2 * cur_radius; // Fixed type underflow

//         while (cx <= cy) {
//           setPixel({ uint16_t(cx_left  - cy), uint16_t(cy_top - cx) }, color);
//           setPixel({ uint16_t(cx_left  - cx), uint16_t(cy_top - cy) }, color);
//           setPixel({ uint16_t(cx_right + cx), uint16_t(cy_top - cy) }, color);
//           setPixel({ uint16_t(cx_right + cy), uint16_t(cy_top - cx) }, color);
//           setPixel({ uint16_t(cx_left  - cy), uint16_t(cy_bot + cx) }, color);
//           setPixel({ uint16_t(cx_left  - cx), uint16_t(cy_bot + cy) }, color);
//           setPixel({ uint16_t(cx_right + cx), uint16_t(cy_bot + cy) }, color);
//           setPixel({ uint16_t(cx_right + cy), uint16_t(cy_bot + cx) }, color);

//           if (d < 0) { 
//             d += 4 * cx + 6; 
//           } else { 
//             d += 4 * (cx - cy) + 10; 
//             cy--; 
//           }
//           ++cx;
//         }
//       }
//     }
//   }
// }

// auto Screen::drawArc(uint16_t xMid, uint16_t yMid, uint8_t radius, Corner corner, uint8_t color)
// -> void {
//   int16_t f     = 1 - radius;
//   int16_t ddF_x = 1;
//   int16_t ddF_y = -2 * radius;
//   int16_t x     = 0;
//   int16_t y     = radius;

//   while (x < y) {
//     if (f >= 0) {
//       y--;
//       ddF_y += 2;
//       f += ddF_y;
//     }
//     x++;
//     ddF_x += 2;  
//     f += ddF_x;  

//     switch (corner) {
//     case Corner::TOP_LEFT:
//       setPixel({(uint16_t)(xMid - x), (uint16_t)(yMid - y)}, color);
//       setPixel({(uint16_t)(xMid - y), (uint16_t)(yMid - x)}, color);
//       break;

//     case Corner::TOP_RIGHT:
//       setPixel({(uint16_t)(xMid + x), (uint16_t)(yMid - y)}, color);
//       setPixel({(uint16_t)(xMid + y), (uint16_t)(yMid - x)}, color);
//       break;

//     case Corner::LOWER_LEFT:
//       setPixel({(uint16_t)(xMid - x), (uint16_t)(yMid + y)}, color);
//       setPixel({(uint16_t)(xMid - y), (uint16_t)(yMid + x)}, color);
//       break;

//     case Corner::LOWER_RIGHT:
//       setPixel({(uint16_t)(xMid + x), (uint16_t)(yMid + y)}, color);
//       setPixel({(uint16_t)(xMid + y), (uint16_t)(yMid + x)}, color);
//       break;
//     }
//   }
// }

// void Screen::drawRoundRectangle(Dim dim, Pos pos,
//                                 uint8_t color) //, bool show)
// {
//   uint16_t xMax = pos.x + dim.width;
//   uint16_t yMax = pos.y + dim.height;

//   if (yMax > height) { yMax = height; }
//   if (xMax > width) { xMax = width; }

//   if (pixelResolution == PixelResolution::MONO) {
//     color = (color == Color::BLACK) ? platform.getEInk().getMonoBlack() : platform.getEInk().getMonoWhite();
//   } else {
//     color = (color == Color::BLACK) ? platform.getEInk().getGrayBlack() : platform.getEInk().getGrayWhite();
//   }

//   for (uint16_t i = pos.x + 10; i < xMax - 10; ++i) {
//     setPixel({i, pos.y}, color);
//     setPixel({i, (uint16_t)(yMax - 1)}, color);
//   }
//   for (uint16_t j = pos.y + 10; j < yMax - 10; ++j) {
//     setPixel({pos.x, j}, color);
//     setPixel({(uint16_t)(xMax - 1), j}, color);
//   }

//   drawArc(pos.x + 10,             pos.y + 10,              10, Corner::TOP_LEFT,    color);
//   drawArc(pos.x + dim.width - 11, pos.y + 10,              10, Corner::TOP_RIGHT,   color);
//   drawArc(pos.x + 10,             pos.y + dim.height - 11, 10, Corner::LOWER_LEFT,  color);
//   drawArc(pos.x + dim.width - 11, pos.y + dim.height - 11, 10, Corner::LOWER_RIGHT, color);
// }

// High performance Colorize a rectangle in the frame buffer
// Does not work with Inkplate-10 and 10_V2 as the screen width is not a multiple of 32.

#if !(INKPLATE_10 || INKPLATE_10_V2)

  auto Screen::lowColorizeGray(Dim dim, Pos pos, uint8_t color) -> void {
    uint32_t *temp =
      ((uint32_t *)(&(frameBufferGray->getData())[frameBufferGray->getStride() * pos.y])) +
      (pos.x >> 3);

    uint32_t colorMask;
    uint16_t lineSize32 = frameBufferGray->getStride() >> 2; // 32 bits count
    // int16_t  inSize32   = (static_cast<int16_t>(dim.width) - (8 - (pos.x & 0x07)) -
    // ((static_cast<int16_t>(dim.width) + pos.x - 1) & 0x07) + 1) >> 3;
    int16_t inSize32 =
      ((dim.width + (pos.x & 0x07) + (8 - ((dim.width + pos.x - 1) & 0x07))) >> 3) - 2;
    uint16_t remainingLineWidth32 = lineSize32 - inSize32 - 2;

    LOG_D("Line Size: {}, inSize: {}, remaining: {}", lineSize32, inSize32,
          remainingLineWidth32);
    LOG_D("Pos: [{}, {}], Dim: [{}, {}]", pos.x, pos.y, dim.width, dim.height);

    colorMask = color | (color << 4);
    colorMask |= (colorMask << 8);
    colorMask |= (colorMask << 16);

    #if GRAY_4_BITS
      uint32_t firstMask = ~(0xFFFFFFFF << ((pos.x & 0x07) << 2));
      uint32_t lastMask  = ~(0xFFFFFFFF >> ((8 - ((pos.x + dim.width) & 0x07)) << 2));
   #else
      uint32_t firstMask = ~(0x77777777 << ((pos.x & 0x07) << 2));
      uint32_t lastMask  = ~(0x77777777 >> ((8 - ((pos.x + dim.width) & 0x07)) << 2));
    #endif

    LOG_D("Masks: {:#010x}, {:010x}, {:010x}", firstMask, lastMask, colorMask);

    if (inSize32 < 0) { // All pixels in a row are in the same 32 bits word
      firstMask &= lastMask;
      colorMask &= ~firstMask;
      for (uint16_t i = 0; i < dim.height; ++i) {
        *temp = (*temp & firstMask) | colorMask;
        temp += lineSize32;
      }
    } else {
      for (uint16_t i = 0; i < dim.height; ++i) {
        *temp = (*temp & firstMask) | (colorMask & ~firstMask);
        temp++;
        for (int16_t j = 0; j < inSize32; ++j) {
          *temp++ = colorMask;
        }
        *temp = (*temp & lastMask) | (colorMask & ~lastMask);
        temp += remainingLineWidth32 + 1;
      }
    }
  }

  auto Screen::lowColorizeMono(Dim dim, Pos pos, uint8_t color) -> void {
    uint32_t *temp =
      ((uint32_t *)(&(frameBufferMono->getData())[frameBufferMono->getStride() * pos.y])) +
      (pos.x >> 5);

    uint16_t lineSize32 = frameBufferMono->getStride() >> 2; // 32 bits count
    int16_t  inSize32 =
      ((dim.width + (pos.x & 0x1F) + (32 - ((dim.width + pos.x - 1) & 0x1F))) >> 5) - 2;
    uint16_t remainingLineWidth32 = lineSize32 - inSize32 - 2;

    LOG_D("Line Size: {}, inSize: {}, remaining: {}", lineSize32, inSize32,
          remainingLineWidth32);
    LOG_D("Pos: [{}, {}], Dim: [{}, {}]", pos.x, pos.y, dim.width, dim.height);

    if (color != 0) {
      uint32_t firstMask = 0xFFFFFFFF << (pos.x & 0x1F);
      uint32_t lastMask  = 0xFFFFFFFF >> (32 - ((pos.x + dim.width) & 0x1F));

      LOG_D("Masks: {:#010x}, {:#010x}", firstMask, lastMask);

      if (inSize32 < 0) {
        firstMask &= lastMask;
        for (uint16_t i = 0; i < dim.height; ++i) {
          *temp |= firstMask;
          temp += lineSize32;
        }
      } else {
        for (uint16_t i = 0; i < dim.height; ++i) {
          *temp++ |= firstMask;
          for (int16_t j = 0; j < inSize32; ++j) {
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
        for (uint16_t i = 0; i < dim.height; ++i) {
          *temp &= firstMask;
          temp += lineSize32;
        }
      } else {
        for (uint16_t i = 0; i < dim.height; ++i) {
          *temp++ &= firstMask;
          for (int16_t j = 0; j < inSize32; ++j) {
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
    if (pixelResolution == PixelResolution::MONO) {
      color = (color == Color::BLACK) ? platform.getEInk().getMonoBlack() : platform.getEInk().getMonoWhite();

      switch (orientation) {
      case Orientation::BOTTOM:
        lowColorizeMono(dim, pos, color);
        break;
      case Orientation::TOP:
        lowColorizeMono(dim, Pos(width - (pos.x + dim.width), height - (pos.y + dim.height)), color);
        break;
      case Orientation::LEFT:
        lowColorizeMono(Dim(dim.height, dim.width), Pos(pos.y, width - (pos.x + dim.width)), color);
        break;
      case Orientation::RIGHT:
        lowColorizeMono(Dim(dim.height, dim.width), Pos(height - (pos.y + dim.height), pos.x), color);
        break;
      }
    } else {
      color = (color == Color::BLACK) ? platform.getEInk().getGrayBlack() : platform.getEInk().getGrayWhite();
      switch (orientation) {
      case Orientation::BOTTOM:
        lowColorizeGray(dim, pos, color);
        break;
      case Orientation::TOP:
        lowColorizeGray(dim, Pos(width - (pos.x + dim.width), height - (pos.y + dim.height)), color);
        break;
      case Orientation::LEFT:
        lowColorizeGray(Dim(dim.height, dim.width), Pos(pos.y, width - (pos.x + dim.width)), color);
        break;
      case Orientation::RIGHT:
        lowColorizeGray(Dim(dim.height, dim.width), Pos(height - (pos.y + dim.height), pos.x), color);
        break;
      }
    }
  #else
    uint16_t xMax = pos.x + dim.width;
    uint16_t yMax = pos.y + dim.height;

    if (yMax > height) { yMax = height; }
    if (xMax > width) { xMax = width; }

    if (pixelResolution == PixelResolution::MONO) {
      color = color == Color::BLACK ? platform.getEInk().getMonoBlack() : platform.getEInk().getMonoWhite();
    } else {
      color = color == Color::BLACK ? platform.getEInk().getGrayBlack() : platform.getEInk().getGrayWhite();
    }

    for (uint16_t j = pos.y; j < yMax; ++j) {
      for (uint16_t i = pos.x; i < xMax; ++i) {
        setPixel({i, j}, color);
      }
    }
  #endif
}

auto Screen::drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch, bool inverse) -> void {
  int xMax = pos.x + dim.width;
  int yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  if (pixelResolution == PixelResolution::MONO) {
    auto color = inverse ? platform.getEInk().getMonoWhite() : platform.getEInk().getMonoBlack();
    for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
      uint32_t p = (q * pitch) << 3;
      for (uint16_t i = pos.x; i < xMax; ++i, ++p) {
        uint8_t v = bitmapData[p >> 3] &LUT_MONO[p & 7];
        if (v) setPixel({i, j}, color);
      }
    }
  } else {
    for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
      uint32_t p = q * pitch;
      for (uint16_t i = pos.x; i < xMax; ++i, ++p) {
        #if GRAY_4_BITS
          uint8_t v = 15 - (bitmapData[p] >> 4);
          if (v != 15) setPixel({i, j}, inverse ? 15 - v : v);
        #else
          uint8_t v = 15 - (bitmapData[p] >> 5);
          if (v != 7) setPixel({i, j}, inverse ? 7 - v : v);
        #endif
      }
    }
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
    if (pixelResolution == PixelResolution::MONO) {

      if (frameBufferGray != nullptr) {
        free(frameBufferGray);
        frameBufferGray = nullptr;
      }

      if ((frameBufferMono = platform.getEInk().newFrameBufferMono()) != nullptr) {
        frameBufferMono->clear();
      }
    } else {
      if (frameBufferMono != nullptr) {
        free(frameBufferMono);
        frameBufferMono = nullptr;
      }

      if ((frameBufferGray = platform.getEInk().newFrameBufferGray()) != nullptr) {
        frameBufferGray->clear();
      }
    }
    
    updateCount = 0;
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
    if (pixelResolution == PixelResolution::MONO) {
      yOffset = ((height + 7) & 0xFFF8) - height;
    } else {
      yOffset = ((height + 1) & 0xFFFE) - height;
    }
  }

  LOG_I("Setting yOffset to {}", yOffset);
}

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

auto Screen::setPixel(Pos pos, uint8_t color) -> void {
  #define CODE(resolution, orientation)             \
      setPixel ## orientation ## resolution(pos, color)

  if (pixelResolution == PixelResolution::MONO) {
    SELECT(Mono);
  } else {
    SELECT(Gray)
  }
}

#undef CODE
#undef SELECT

auto Screen::setPixelLeftMono(Pos pos, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferMono->getData())[frameBufferMono->getDataSize() -
      (frameBufferMono->getStride() * (pos.x + 1)) + (pos.y >> 3)];
  #if PIXELS_BIG_ENDIAN
    if (color != 0) {
      *temp = *temp | LUT_MONO[pos.y & 7];
    }
    else {
      *temp = (*temp & ~LUT_MONO[pos.y & 7]);
    }
  #else
    if (color != 0) {
      *temp = *temp | LUT_MONO_INV[pos.y & 7];
    }
    else {
      *temp = (*temp & ~LUT_MONO_INV[pos.y & 7]);
    }
  #endif
}

auto Screen::setPixelRightMono(Pos pos, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferMono
      ->getData())[(frameBufferMono->getStride() * (pos.x + 1)) - (pos.y >> 3) - 1];

  #if PIXELS_BIG_ENDIAN
    if (color != 0) {
      *temp = (*temp | LUT_MONO_INV[pos.y & 7]);
    }
    else {
      *temp = (*temp & ~LUT_MONO_INV[pos.y & 7]);
    }
  #else
    if (color != 0) {
      *temp = *temp | LUT_MONO[pos.y & 7];
    }
    else {
      *temp = (*temp & ~LUT_MONO[pos.y & 7]);
    }
  #endif
}

auto Screen::setPixelBottomMono(Pos pos, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferMono->getData())[frameBufferMono->getStride() * pos.y + (pos.x >> 3)];

  #if PIXELS_BIG_ENDIAN
    if (color != 0) {
      *temp = *temp | LUT_MONO[pos.x & 7];
    }
    else {
      *temp = (*temp & ~LUT_MONO[pos.x & 7]);
    }
  #else
    if (color != 0) {
      *temp = *temp | LUT_MONO_INV[pos.x & 7];
    }
    else {
      *temp = (*temp & ~LUT_MONO_INV[pos.x & 7]);
    }
  #endif
}

auto Screen::setPixelTopMono(Pos pos, uint8_t color) -> void {
  uint8_t *temp = &(frameBufferMono->getData())[frameBufferMono->getDataSize() -
                    (frameBufferMono->getStride() * pos.y) - ((pos.x + yOffset) >> 3)];

  #if PIXELS_BIG_ENDIAN
    if (color != 0) {
      *temp = *temp | LUT_MONO_INV[((pos.x + yOffset) & 7)];
    } else {
      *temp = (*temp & ~LUT_MONO_INV[((pos.x + yOffset) & 7)]);
    }
  #else
    if (color != 0) {
      *temp = *temp | LUT_MONO[(pos.x + yOffset) & 7];
    } else {
      *temp = (*temp & ~LUT_MONO[(pos.x + yOffset) & 7]);
    }
  #endif
}

auto Screen::setPixelLeftGray(Pos pos, uint8_t color) -> void {
  uint8_t *temp = &(frameBufferGray->getData())[frameBufferGray->getDataSize() -
                  (frameBufferGray->getStride() * (pos.x + 1)) + (pos.y >> 1)];
  #if PIXELS_BIG_ENDIAN
    *temp = (pos.y & 1) ? ((*temp & 0xF0) | color) : ((*temp & 0x0F) | (color << 4));
  #else
    *temp = (pos.y & 1) ? ((*temp & 0x0F) | (color << 4)) : ((*temp & 0xF0) | color);
  #endif
}


auto Screen::setPixelRightGray(Pos pos, uint8_t color) -> void {
  uint8_t *temp = &(frameBufferGray->getData())[(frameBufferGray->getStride() *
                  (pos.x + 1)) - (pos.y >> 1) - 1];
  #if PIXELS_BIG_ENDIAN
    *temp = (pos.y & 1) ? ((*temp & 0xF0) | color) : ((*temp & 0x0F) | (color << 4));
  #else
    *temp = (pos.y & 1) ? ((*temp & 0x0F) | (color << 4)) : ((*temp & 0xF0) | color);
  #endif
}

auto Screen::setPixelBottomGray(Pos pos, uint8_t color) -> void {
  uint8_t *temp = &(frameBufferGray->getData())[frameBufferGray->getStride() * 
                  pos.y + (pos.x >> 1)];
  if (pos.x & 1) {
    *temp = (*temp & 0xF0) | color;
  } else {
    *temp = (*temp & 0x0F) | (color << 4);
  }
}

auto Screen::setPixelTopGray(Pos pos, uint8_t color) -> void {
  uint8_t *temp =
    &(frameBufferGray->getData())[frameBufferGray->getDataSize() -
                                    (frameBufferGray->getStride() * pos.y) - ((pos.x + yOffset) >> 1)];
  if ((pos.x + yOffset) & 1) {
    *temp = (*temp & 0x0F) | (color << 4);
  } else {
    *temp = (*temp & 0xF0) | color;
  }
}