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

  if (pos.x > width ) pos.x = 0;
  if (pos.y > height) pos.y = 0;

  uint32_t x_max = pos.x + dim.width;
  uint32_t y_max = pos.y + dim.height;

  if (y_max > height) y_max = height;
  if (x_max > width ) x_max = width;

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

  if (y_max > height) y_max = height;
  if (x_max > width ) x_max = width;
  
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
    color = color == Color::BLACK ? 1 : 0;
    SELECT(1bit);
  }
  else {
    #if INKPLATE_6FLICK
        color = color == Color::BLACK ? 0 : 7;
    #endif
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

  if (pixel_resolution == PixelResolution::ONE_BIT) { 
    SELECT(1bit); 
  } 
  else { 
    SELECT(3bit) 
  };

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

  if (y_max > height) y_max = height;
  if (x_max > width ) x_max = width;

  #define CODE(resolution, orientation)                            \
  for (int i = pos.x + 10; i < x_max - 10; i++) {                  \
    set_pixel_o_##orientation##_##resolution(i, pos.y,     color); \
    set_pixel_o_##orientation##_##resolution(i, y_max - 1, color); \
  }                                                                \
  for (int j = pos.y + 10; j < y_max - 10; j++) {                  \
    set_pixel_o_##orientation##_##resolution(pos.x,     j, color); \
    set_pixel_o_##orientation##_##resolution(x_max - 1, j, color); \
  }

  if (pixel_resolution == PixelResolution::ONE_BIT) { 
    color = color == Color::BLACK ? 1 : 0;
    SELECT(1bit); 
  } 
  else { 
    #if INKPLATE_6FLICK
        color = color == Color::BLACK ? 0 : 7;
    #endif
    SELECT(3bit); 
  }

  #undef CODE

  draw_arc(pos.x + 10,             pos.y + 10,              10, Corner::TOP_LEFT,    color);
  draw_arc(pos.x + dim.width - 11, pos.y + 10,              10, Corner::TOP_RIGHT,   color);
  draw_arc(pos.x + 10,             pos.y + dim.height - 11, 10, Corner::LOWER_LEFT,  color);
  draw_arc(pos.x + dim.width - 11, pos.y + dim.height - 11, 10, Corner::LOWER_RIGHT, color);
}

// High performance Colorize a rectangle in the frame buffer
// Does not work with Inkplate-10 as the screen width is not a multiple of 32.

#if !INKPLATE_10

  void Screen::low_colorize_3bit(Dim dim, Pos pos, uint8_t color)
  {
    uint32_t * temp = ((uint32_t *)(&(frame_buffer_3bit->get_data())[frame_buffer_3bit->get_line_size() * pos.y])) + (pos.x >> 3);
  
    uint32_t color_mask;
    uint16_t line_size_32 = frame_buffer_3bit->get_line_size() >> 2; // 32 bits count
    //int16_t  in_size_32   = (((int16_t) dim.width) - (8 - (pos.x & 0x07)) - ((((int16_t) dim.width) + pos.x - 1) & 0x07) + 1) / 8;
    int16_t  in_size_32   = ((dim.width + (pos.x & 0x07) + (8 - ((dim.width + pos.x - 1) & 0x07))) >> 3) - 2;
    uint16_t remaining_line_width_32 = line_size_32 - in_size_32 - 2;

    LOG_D("Line Size: %u, in_size: %d, remaining: %u", line_size_32, in_size_32, remaining_line_width_32);
    LOG_D("Pos: [%d, %d], Dim: [%d, %d]", pos.x, pos.y, dim.width, dim.height);

    color_mask = color | (color << 4);
    color_mask |= (color_mask <<  8);
    color_mask |= (color_mask << 16);

    uint32_t first_mask = ~(0x77777777 << ((pos.x & 0x07) << 2));
    uint32_t last_mask =  ~(0x77777777 >> ((8 - ((pos.x + dim.width) & 0x07)) << 2));

    LOG_D("Masks: %08" PRIX32 ", %08" PRIX32 ", %08" PRIX32, first_mask, last_mask, color_mask);

    if (in_size_32 < 0) { // All pixels in a row are in the same 32 bits word
      first_mask &= last_mask;
      color_mask &= ~first_mask;
      for (uint16_t i = 0; i < dim.height; i++) {
        *temp = (*temp & first_mask) | color_mask;
        temp += line_size_32;
      }
    }
    else for (uint16_t i = 0; i < dim.height; i++) {
      *temp = (*temp & first_mask) | (color_mask & ~first_mask); 
      temp++;
      for (int16_t j = 0; j < in_size_32; j++) {
        *temp++ = color_mask;
      }
      *temp = (*temp & last_mask) | (color_mask & ~last_mask); 
      temp += remaining_line_width_32 + 1;
    }
  }

  void Screen::low_colorize_1bit(Dim dim, Pos pos, uint8_t color)
  { 
    uint32_t * temp = ((uint32_t *)(&(frame_buffer_1bit->get_data())[frame_buffer_1bit->get_line_size() * pos.y])) + (pos.x >> 5);
  
    uint16_t line_size_32 = frame_buffer_1bit->get_line_size() >> 2; // 32 bits count
    int16_t  in_size_32   = ((dim.width + (pos.x & 0x1F) + (32 - ((dim.width + pos.x - 1) & 0x1F))) >> 5) - 2;
    uint16_t remaining_line_width_32 = line_size_32 - in_size_32 - 2;

    LOG_D("Line Size: %u, in_size: %d, remaining: %u", line_size_32, in_size_32, remaining_line_width_32);
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
      }
      else for (uint16_t i = 0; i < dim.height; i++) {
        *temp++ |= first_mask;
        for (int16_t j = 0; j < in_size_32; j++) {
          *temp++ = 0xFFFFFFFF;
        }
        *temp++ |= last_mask;
        temp += remaining_line_width_32;
      }
    }
    else {
      uint32_t first_mask = ~(0xFFFFFFFF << (pos.x & 0x1F));
      uint32_t last_mask =  ~(0xFFFFFFFF >> (32 - ((pos.x + dim.width) & 0x1F)));

      LOG_D("Masks: %08" PRIX32 ", %08" PRIX32, first_mask, last_mask);

      if (in_size_32 < 0) {
        first_mask |= last_mask;
        for (uint16_t i = 0; i < dim.height; i++) {
          *temp &= first_mask;
          temp += line_size_32;
        }
      }
      else for (uint16_t i = 0; i < dim.height; i++) {
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

void
Screen::colorize_region(
  Dim     dim,
  Pos     pos,
  uint8_t color) //, bool show)
{
  #if !INKPLATE_10
  if (pixel_resolution == PixelResolution::ONE_BIT) {
    color = color == Color::BLACK ? 1 : 0;

    switch (orientation) {
      case Orientation::BOTTOM:
        low_colorize_1bit(dim, pos, color);
        break;
      case Orientation::TOP:
        low_colorize_1bit(dim, Pos(width - (pos.x + dim.width), height - (pos.y + dim.height)), color);
        break;
      case Orientation::LEFT:
        low_colorize_1bit(Dim(dim.height, dim.width), Pos(pos.y, width - (pos.x + dim.width)), color);
        break;
      case Orientation::RIGHT:
        low_colorize_1bit(Dim(dim.height, dim.width), Pos(height - (pos.y + dim.height), pos.x), color);
        break;
    }
  }
  else {
    #if INKPLATE_6FLICK
        color = color == Color::BLACK ? 0 : 7;
    #endif
    switch (orientation) {
      case Orientation::BOTTOM:
        low_colorize_3bit(dim, pos, color);
        break;
      case Orientation::TOP:
        low_colorize_3bit(dim, Pos(width - (pos.x + dim.width), height - (pos.y + dim.height)), color);
        break;
      case Orientation::LEFT:
        low_colorize_3bit(Dim(dim.height, dim.width), Pos(pos.y, width - (pos.x + dim.width)), color);
        break;
      case Orientation::RIGHT:
        low_colorize_3bit(Dim(dim.height, dim.width), Pos(height - (pos.y + dim.height), pos.x), color);
        break;
    }
  }

#else  
  int16_t x_max = pos.x + dim.width;
  int16_t y_max = pos.y + dim.height;

  if (y_max > height) y_max = height;
  if (x_max > width ) x_max = width;

  #define CODE(resolution, orientation)                        \
    for (int j = pos.y; j < y_max; j++) {                      \
      for (int i = pos.x; i < x_max; i++) {                    \
        set_pixel_o_##orientation##_##resolution(i, j, color); \
      }                                                        \
    }

  if (pixel_resolution == PixelResolution::ONE_BIT) {
    color = color == Color::BLACK ? 1 : 0;
    SELECT(1bit);
  }
  else {
    SELECT(3bit);
  }

  #undef CODE

#endif
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

  if (y_max > height) y_max = height;
  if (x_max > width ) x_max = width;

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
    width  = e_ink.get_height();
    height = e_ink.get_width();
  }
  else {
    width  = e_ink.get_width();
    height = e_ink.get_height();
  }
}