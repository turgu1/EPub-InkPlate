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
#include <cstring>

#define BYTES_PER_PIXEL 3

Screen Screen::singleton;

uint16_t Screen::WIDTH;
uint16_t Screen::HEIGHT;

const uint8_t Screen::LUT1BIT[8]     = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
const uint8_t Screen::LUT1BIT_INV[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

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
    static int16_t err[801];
    int16_t error;
    memset(err, 0, 801*2);

    if (orientation == Orientation::LEFT) {
      for (int j = pos.y, q = 0; j < y_max; j++, q++) {
        for (int i = pos.x, p = q * dim.width, k = 0; i < (x_max - 1); i++, p++, k++) {
          int32_t v = bitmap_data[p] + err[k + 1];
          if (v > 128) {
            error = (v - 255);
            set_pixel_o_left_1bit(i, j, 0);
          }
          else {
            error = v;
            set_pixel_o_left_1bit(i, j, 1);
          }
          if (k != 0) {
            err[k - 1] += error / 8;
          }
          err[k]     += 3 * error / 8;
          err[k + 1]  =     error / 8;
          err[k + 2] += 3 * error / 8;
        }
      }
    }
    else if (orientation == Orientation::RIGHT) {
      for (int j = pos.y, q = 0; j < y_max; j++, q++) {
        for (int i = pos.x, p = q * dim.width, k = 0; i < (x_max - 1); i++, p++, k++) {
          int32_t v = bitmap_data[p] + err[k + 1];
          if (v > 128) {
            error = (v - 255);
            set_pixel_o_right_1bit(i, j, 0);
          }
          else {
            error = v;
            set_pixel_o_right_1bit(i, j, 1);
          }
          if (k != 0) {
            err[k - 1] += error / 8;
          }
          err[k]     += 3 * error / 8;
          err[k + 1]  =     error / 8;
          err[k + 2] += 3 * error / 8;
        }
      }
    }
    else { // Orientation::BOTTOM
      for (int j = pos.y, q = 0; j < y_max; j++, q++) {
        for (int i = pos.x, p = q * dim.width, k = 0; i < (x_max - 1); i++, p++, k++) {
          int32_t v = bitmap_data[p] + err[k + 1];
          if (v > 128) {
            error = (v - 255);
            set_pixel_o_bottom_1bit(i, j, 0);
          }
          else {
            error = v;
            set_pixel_o_bottom_1bit(i, j, 1);
          }
          if (k != 0) {
            err[k - 1] += error / 8;
          }
          err[k]     += 3 * error / 8;
          err[k + 1]  =     error / 8;
          err[k + 2] += 3 * error / 8;
        }
      }
    }
  }
  else {
    if (orientation == Orientation::LEFT) {
      for (int j = pos.y, q = 0; j < y_max; j++, q++) {
        for (int i = pos.x, p = q * dim.width; i < x_max; i++, p++) {
          int8_t v = bitmap_data[p] >> 5;
          set_pixel_o_left_3bit(i, j, v);
        }
      }
    }
    else if (orientation == Orientation::RIGHT) {
      for (int j = pos.y, q = 0; j < y_max; j++, q++) {
        for (int i = pos.x, p = q * dim.width; i < x_max; i++, p++) {
          int8_t v = bitmap_data[p] >> 5;
          set_pixel_o_right_3bit(i, j, v);
        }
      }
    }
    else { // BOTTOM
      for (int j = pos.y, q = 0; j < y_max; j++, q++) {
        for (int i = pos.x, p = q * dim.width; i < x_max; i++, p++) {
          int8_t v = bitmap_data[p] >> 5;
          set_pixel_o_bottom_3bit(i, j, v);
        }
      }
    }
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
  
  if (pixel_resolution == PixelResolution::ONE_BIT) {
    uint8_t col = color == BLACK_COLOR ? 1 : 0;
  
    if (orientation == Orientation::LEFT) {
      for (int i = pos.x; i < x_max; i++) {
        set_pixel_o_left_1bit(i, pos.y, col);
        set_pixel_o_left_1bit(i, y_max - 1, col);
      }
      for (int j = pos.y; j < y_max; j++) {
        set_pixel_o_left_1bit(pos.x, j, col);
        set_pixel_o_left_1bit(x_max - 1, j, col);
      }
    }
    else if (orientation == Orientation::RIGHT) {
      for (int i = pos.x; i < x_max; i++) {
        set_pixel_o_right_1bit(i, pos.y, col);
        set_pixel_o_right_1bit(i, y_max - 1, col);
      }
      for (int j = pos.y; j < y_max; j++) {
        set_pixel_o_right_1bit(pos.x, j, col);
        set_pixel_o_right_1bit(x_max - 1, j, col);
      }
    }
    else {
      for (int i = pos.x; i < x_max; i++) {
        set_pixel_o_bottom_1bit(i, pos.y, col);
        set_pixel_o_bottom_1bit(i, y_max - 1, col);
      }
      for (int j = pos.y; j < y_max; j++) {
        set_pixel_o_bottom_1bit(pos.x, j, col);
        set_pixel_o_bottom_1bit(x_max - 1, j, col);
      }
    }
  }
  else {
    if (orientation == Orientation::LEFT) {
      for (int i = pos.x; i < x_max; i++) {
        set_pixel_o_left_3bit(i, pos.y, color);
        set_pixel_o_left_3bit(i, y_max - 1, color);
      }
      for (int j = pos.y; j < y_max; j++) {
        set_pixel_o_left_3bit(pos.x, j, color);
        set_pixel_o_left_3bit(x_max - 1, j, color);
      }
    }
    else if (orientation == Orientation::RIGHT) {
      for (int i = pos.x; i < x_max; i++) {
        set_pixel_o_right_3bit(i, pos.y, color);
        set_pixel_o_right_3bit(i, y_max - 1, color);
      }
      for (int j = pos.y; j < y_max; j++) {
        set_pixel_o_right_3bit(pos.x, j, color);
        set_pixel_o_right_3bit(x_max - 1, j, color);
      }
    }
    else {
      for (int i = pos.x; i < x_max; i++) {
        set_pixel_o_bottom_3bit(i, pos.y, color);
        set_pixel_o_bottom_3bit(i, y_max - 1, color);
      }
      for (int j = pos.y; j < y_max; j++) {
        set_pixel_o_bottom_3bit(pos.x, j, color);
        set_pixel_o_bottom_3bit(x_max - 1, j, color);
      }
    }
  }
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

  if (pixel_resolution == PixelResolution::ONE_BIT) {
    int8_t col = color == BLACK_COLOR ? 1 : 0;
    if (orientation == Orientation::LEFT) {
      for (int j = pos.y; j < y_max; j++) {
        for (int i = pos.x; i < x_max; i++) {
          set_pixel_o_left_1bit(i, j, color);
        }
      }
    }
    else if (orientation == Orientation::RIGHT) {
      for (int j = pos.y; j < y_max; j++) {
        for (int i = pos.x; i < x_max; i++) {
          set_pixel_o_right_1bit(i, j, col);
        }
      }
    }
    else {
      for (int j = pos.y; j < y_max; j++) {
        for (int i = pos.x; i < x_max; i++) {
          set_pixel_o_bottom_1bit(i, j, col);
        }
      }
    }
  }
  else {
    if (orientation == Orientation::LEFT) {
      for (int j = pos.y; j < y_max; j++) {
        for (int i = pos.x; i < x_max; i++) {
          set_pixel_o_left_3bit(i, j, color);
        }
      }
    }
    else if (orientation == Orientation::RIGHT) {
      for (int j = pos.y; j < y_max; j++) {
        for (int i = pos.x; i < x_max; i++) {
          set_pixel_o_right_3bit(i, j, color);
        }
      }
    }
    else {
      for (int j = pos.y; j < y_max; j++) {
        for (int i = pos.x; i < x_max; i++) {
          set_pixel_o_bottom_3bit(i, j, color);
        }
      }
    }
  }
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
    if (orientation == Orientation::LEFT) {
      for (uint32_t j = pos.y, q = 0; j < y_max; j++, q++) {  // row
        for (uint32_t i = pos.x, p = (q * pitch) << 3; i < x_max; i++, p++) { // column
          uint8_t v = bitmap_data[p >> 3] & LUT1BIT[p & 7];
          set_pixel_o_left_1bit(i, j, v ? 1 : 0);
        }
      }
    }
    else if (orientation == Orientation::RIGHT) {
      for (uint32_t j = pos.y, q = 0; j < y_max; j++, q++) {  // row
        for (uint32_t i = pos.x, p = (q * pitch) << 3; i < x_max; i++, p++) { // column
          uint8_t v = bitmap_data[p >> 3] & LUT1BIT[p & 7];
          set_pixel_o_right_1bit(i, j, v ? 1 : 0);
        }
      }
    }
    else {
      for (uint32_t j = pos.y, q = 0; j < y_max; j++, q++) {  // row
        for (uint32_t i = pos.x, p = (q * pitch) << 3; i < x_max; i++, p++) { // column
          uint8_t v = bitmap_data[p >> 3] & LUT1BIT[p & 7];
          set_pixel_o_bottom_1bit(i, j, v ? 1 : 0);
        }
      }
    }
  }
  else {
    if (orientation == Orientation::LEFT) {
      for (uint32_t j = pos.y, q = 0; j < y_max; j++, q++) {  // row
        for (uint32_t i = pos.x, p = q * pitch; i < x_max; i++, p++) { // column
          uint8_t v = 7 - (bitmap_data[p] >> 5);
          set_pixel_o_left_3bit(i, j, v);
        }
      }
    }
    else if (orientation == Orientation::RIGHT) {
      for (uint32_t j = pos.y, q = 0; j < y_max; j++, q++) {  // row
        for (uint32_t i = pos.x, p = q * pitch; i < x_max; i++, p++) { // column
          uint8_t v = 7 - (bitmap_data[p] >> 5);
          set_pixel_o_right_3bit(i, j, v);
        }
      }
    }
    else {
      for (uint32_t j = pos.y, q = 0; j < y_max; j++, q++) {  // row
        for (uint32_t i = pos.x, p = q * pitch; i < x_max; i++, p++) { // column
          uint8_t v = 7 - (bitmap_data[p] >> 5);
          set_pixel_o_bottom_3bit(i, j, v);
        }
      }
    }
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
    WIDTH  = 600;
    HEIGHT = 800;
  }
  else {
    WIDTH  = 800;
    HEIGHT = 600;
  }
}
