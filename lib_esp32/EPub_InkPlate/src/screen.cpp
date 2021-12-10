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

uint16_t Screen::WIDTH;
uint16_t Screen::HEIGHT;

const uint8_t Screen::LUT1BIT[8]     = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
const uint8_t Screen::LUT1BIT_INV[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

#define SELECT(resolution)                         \
  if (orientation == Orientation::LEFT) {          \
    CODE(resolution, left);                        \
  } else if (orientation == Orientation::RIGHT) {  \
    CODE(resolution, right);                       \
  } else if (orientation == Orientation::BOTTOM) { \
    CODE(resolution, bottom);                      \
  } else {                                         \
    CODE(resolution, top);                         \
  }

void 
Screen::draw_bitmap(
  const unsigned char * bitmap_data, 
  Dim                   dim,
  Pos                   pos)
{
  if (bitmap_data == nullptr) return;
  
  // if (show) {
  //   unsigned char * p = bitmap_data;
  //   for (int j = 0; j < image_height; j++) {
  //     for (int i = 0; i < image_width; i++) {
  //       std::cout << std::setw(2) << std::setfill('0') << std::hex << (int) *p++;
  //     }
  //     std::cout << std::endl;
  //   }
  // }   

  if (pos.x < 0) pos.x = 0;
  if (pos.y < 0) pos.y = 0;

  uint32_t x_max = pos.x + dim.width;
  uint32_t y_max = pos.y + dim.height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  if (pixel_resolution == PixelResolution::ONE_BIT) {
    static int16_t err[1201]; // This is the maximum width of all Inkplate devices + 1
    int16_t error;
    memset(err, 0, 1201*2);

    #define CODE(resolution, orientation)                      \
      for (int j = pos.y, q = 0; j < y_max; j++, q++) {        \
        for (int i = pos.x, p = q * dim.width, k = 0;          \
             i < (x_max - 1);                                  \
             i++, p++, k++) {                                  \
          int32_t v = bitmap_data[p] + err[k + 1];             \
          if (v > 128) {                                       \
            error = (v - 255);                                 \
            set_pixel_o_##orientation##_##resolution(i, j, 0); \
          }                                                    \
          else {                                               \
            error = v;                                         \
            set_pixel_o_##orientation##_##resolution(i, j, 1); \
          }                                                    \
          if (k != 0) {                                        \
            err[k - 1] += error / 8;                           \
          }                                                    \
          err[k]     += 3 * error / 8;                         \
          err[k + 1]  =     error / 8;                         \
          err[k + 2] += 3 * error / 8;                         \
        }                                                      \
      }

    SELECT(1bit);
    #undef CODE
  }
  else {
    #define CODE(resolution, orientation)                              \
      for (int j = pos.y, q = 0; j < y_max; j++, q++) {                \
        for (int i = pos.x, p = q * dim.width; i < x_max; i++, p++) {  \
          int8_t v = bitmap_data[p] >> 5;                              \
          set_pixel_o_##orientation##_##resolution(i, j, v);           \
        }                                                              \
      }
    SELECT(3bit);
    #undef CODE
  }
}

void 
Screen::draw_rectangle(
  Dim     dim,
  Pos     pos,
  uint8_t color)
{
  uint32_t x_max = pos.x + dim.width;
  uint32_t y_max = pos.y + dim.height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;
  
  #define CODE(resolution, orientation)                              \
    for (int i = pos.x; i < x_max; i++) {                            \
      set_pixel_o_##orientation##_##resolution(i, pos.y, color);     \
      set_pixel_o_##orientation##_##resolution(i, y_max - 1, color); \
    }                                                                \
    for (int j = pos.y; j < y_max; j++) {                            \
      set_pixel_o_##orientation##_##resolution(pos.x, j, color);     \
      set_pixel_o_##orientation##_##resolution(x_max - 1, j, color); \
    }

  if (pixel_resolution == PixelResolution::ONE_BIT) {
    color = color == BLACK_COLOR ? 1 : 0;
    SELECT(1bit);
  }
  else {
    SELECT(3bit);
  }

  #undef CODE
}

void
Screen::draw_arc(uint16_t x_mid,  uint16_t y_mid,  uint8_t radius, Corner corner, uint8_t color)
{
  int16_t f     =  1 - radius;
  int16_t ddF_x =           1;
  int16_t ddF_y = -2 * radius;
  int16_t x     =           0;
  int16_t y     =      radius;

  #define CODE(resolution, orientation)                                        \
  while ( x < y ) {                                                            \
    if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }                               \
    x++; ddF_x += 2; f += ddF_x;                                               \
                                                                               \
    switch (corner) {                                                          \
      case Corner::TOP_LEFT:                                                   \
        set_pixel_o_##orientation##_##resolution(x_mid - x, y_mid - y, color); \
        set_pixel_o_##orientation##_##resolution(x_mid - y, y_mid - x, color); \
        break;                                                                 \
                                                                               \
      case Corner::TOP_RIGHT:                                                  \
        set_pixel_o_##orientation##_##resolution(x_mid + x, y_mid - y, color); \
        set_pixel_o_##orientation##_##resolution(x_mid + y, y_mid - x, color); \
        break;                                                                 \
                                                                               \
      case Corner::LOWER_LEFT:                                                 \
        set_pixel_o_##orientation##_##resolution(x_mid - x, y_mid + y, color); \
        set_pixel_o_##orientation##_##resolution(x_mid - y, y_mid + x, color); \
        break;                                                                 \
                                                                               \
      case Corner::LOWER_RIGHT:                                                \
        set_pixel_o_##orientation##_##resolution(x_mid + x, y_mid + y, color); \
        set_pixel_o_##orientation##_##resolution(x_mid + y, y_mid + x, color); \
        break;                                                                 \
    }                                                                          \
  }

  if (pixel_resolution == PixelResolution::ONE_BIT) { SELECT(1bit); } else { SELECT(3bit) };

  #undef CODE
}

void 
Screen::draw_round_rectangle(
  Dim      dim,
  Pos      pos,
  uint8_t  color) //, bool show)
{
  int16_t x_max = pos.x + dim.width;
  int16_t y_max = pos.y + dim.height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  #define CODE(resolution, orientation)                            \
  for (int i = pos.x + 10; i < x_max - 10; i++) {                  \
    set_pixel_o_##orientation##_##resolution(i, pos.y,     color); \
    set_pixel_o_##orientation##_##resolution(i, y_max - 1, color); \
  }                                                                \
  for (int j = pos.y + 10; j < y_max - 10; j++) {                  \
    set_pixel_o_##orientation##_##resolution(pos.x,     j, color); \
    set_pixel_o_##orientation##_##resolution(x_max - 1, j, color); \
  }

  if (pixel_resolution == PixelResolution::ONE_BIT) { SELECT(1bit); } else { SELECT(3bit); }

  #undef CODE

  draw_arc(pos.x + 10,             pos.y + 10,              10, Corner::TOP_LEFT,    color);
  draw_arc(pos.x + dim.width - 11, pos.y + 10,              10, Corner::TOP_RIGHT,   color);
  draw_arc(pos.x + 10,             pos.y + dim.height - 11, 10, Corner::LOWER_LEFT,  color);
  draw_arc(pos.x + dim.width - 11, pos.y + dim.height - 11, 10, Corner::LOWER_RIGHT, color);
}

void
Screen::colorize_region(
  Dim     dim,
  Pos     pos,
  uint8_t color) //, bool show)
{
  int16_t x_max = pos.x + dim.width;
  int16_t y_max = pos.y + dim.height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  #define CODE(resolution, orientation)                        \
    for (int j = pos.y; j < y_max; j++) {                      \
      for (int i = pos.x; i < x_max; i++) {                    \
        set_pixel_o_##orientation##_##resolution(i, j, color); \
      }                                                        \
    }

  if (pixel_resolution == PixelResolution::ONE_BIT) {
    color = color == BLACK_COLOR ? 1 : 0;
    SELECT(1bit);
  }
  else {
    SELECT(3bit);
  }

  #undef CODE
}

void 
Screen::draw_glyph(
  const unsigned char * bitmap_data, 
  Dim                   dim,
  Pos                   pos, 
  uint16_t              pitch)
{
  int x_max = pos.x + dim.width;
  int y_max = pos.y + dim.height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  if (pixel_resolution == PixelResolution::ONE_BIT) {
    #define CODE(resolution, orientation)                                     \
      for (uint32_t j = pos.y, q = 0; j < y_max; j++, q++) {                  \
        for (uint32_t i = pos.x, p = (q * pitch) << 3; i < x_max; i++, p++) { \
          uint8_t v = bitmap_data[p >> 3] & LUT1BIT[p & 7];                   \
          if (v) set_pixel_o_##orientation##_##resolution(i, j, 1);           \
        }                                                                     \
      }
    SELECT(1bit);
    #undef CODE
  }
  else {
    #define CODE(resolution, orientation)                                     \
      for (uint32_t j = pos.y, q = 0; j < y_max; j++, q++) {                  \
        for (uint32_t i = pos.x, p = q * pitch; i < x_max; i++, p++) {        \
          uint8_t v = 7 - (bitmap_data[p] >> 5);                              \
          if (v != 7) set_pixel_o_##orientation##_##resolution(i, j, v);      \
        }                                                                     \
      }
    SELECT(3bit);
    #undef CODE
  }
}

void 
Screen::setup(PixelResolution resolution, Orientation orientation)
{
  set_orientation(orientation);
  set_pixel_resolution(resolution, true);
  clear();
}

void 
Screen::set_pixel_resolution(PixelResolution resolution, bool force)
{
  if (force || (pixel_resolution != resolution)) {
    pixel_resolution = resolution;
    if (pixel_resolution == PixelResolution::ONE_BIT) {
      if (frame_buffer_3bit != nullptr) {
        free(frame_buffer_3bit);
        frame_buffer_3bit = nullptr;
      }
      if ((frame_buffer_1bit = e_ink.new_frame_buffer_1bit()) != nullptr) {
        frame_buffer_1bit->clear();
      }
      partial_count = 0;
    }
    else {
      if (frame_buffer_1bit != nullptr) {
        free(frame_buffer_1bit);
        frame_buffer_1bit = nullptr;
      }
      if ((frame_buffer_3bit = e_ink.new_frame_buffer_3bit()) != nullptr) {
        frame_buffer_3bit->clear();
      }
    }
  }
}

void 
Screen::set_orientation(Orientation orient) 
{
  orientation = orient;
  if ((orientation == Orientation::LEFT) || (orientation == Orientation::RIGHT)) {
    WIDTH  = e_ink.get_height();
    HEIGHT = e_ink.get_width();
  }
  else {
    WIDTH  = e_ink.get_width();
    HEIGHT = e_ink.get_height();
  }
}

#if defined(INKPLATE_6PLUS)
  void 
  Screen::to_user_coord(uint16_t & x, uint16_t & y)
  {
    uint16_t temp;
    if (orientation == Orientation::BOTTOM) {
      LOG_D("Bottom...");
      temp = y;
      y = ((uint32_t) (HEIGHT - 1) *                                       x ) / touch_screen.get_x_resolution();
      x = ((uint32_t) (WIDTH  - 1) * (touch_screen.get_y_resolution() - temp)) / touch_screen.get_y_resolution();
    }
    else if (orientation == Orientation::TOP) {
      LOG_D("Top...");
      temp = y;
      y = ((uint32_t) (HEIGHT - 1) * (touch_screen.get_x_resolution() -    x)) / touch_screen.get_x_resolution();
      x = ((uint32_t) (WIDTH  - 1) *                                    temp ) / touch_screen.get_y_resolution();
    }
    else if (orientation == Orientation::LEFT) {
      LOG_D("Left...");
      x = ((uint32_t) (WIDTH  - 1) * (touch_screen.get_x_resolution() - x)) / touch_screen.get_x_resolution();
      y = ((uint32_t) (HEIGHT - 1) * (touch_screen.get_y_resolution() - y)) / touch_screen.get_y_resolution();
    }
    else {
      LOG_D("Right...");
      x = ((uint32_t) (WIDTH  - 1) * x) / touch_screen.get_x_resolution();
      y = ((uint32_t) (HEIGHT - 1) * y) / touch_screen.get_y_resolution();
    }
  }
#endif