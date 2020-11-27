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
Screen::Orientation Screen::orientation;

const uint8_t Screen::LUT1BIT[8]     = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
const uint8_t Screen::LUT1BIT_INV[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

void 
Screen::draw_bitmap(
  const unsigned char * bitmap_data, 
  uint16_t width, 
  uint16_t height, 
  int16_t x, 
  int16_t y)
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

  if (x < 0) x = 0;
  if (y < 0) y = 0;

  uint32_t x_max = x + width;
  uint32_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  static int16_t err[801];
  int16_t error;
  memset(err, 0, 801*2);

  if (orientation == O_LEFT) {
    for (int j = y, q = 0; j < y_max; j++, q++) {
      for (int i = x, p = q * width, k = 0; i < (x_max - 1); i++, p++, k++) {
        int32_t v = bitmap_data[p] + err[k + 1];
        if (v > 128) {
          error = (v - 255);
          set_pixel_o_left(i, j, 0);
        }
        else {
          error = v;
          set_pixel_o_left(i, j, 1);
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
  else if (orientation == O_RIGHT) {
    for (int j = y, q = 0; j < y_max; j++, q++) {
      for (int i = x, p = q * width, k = 0; i < (x_max - 1); i++, p++, k++) {
        int32_t v = bitmap_data[p] + err[k + 1];
        if (v > 128) {
          error = (v - 255);
          set_pixel_o_right(i, j, 0);
        }
        else {
          error = v;
          set_pixel_o_right(i, j, 1);
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
  else {
    for (int j = y, q = 0; j < y_max; j++, q++) {
      for (int i = x, p = q * width, k = 0; i < (x_max - 1); i++, p++, k++) {
        int32_t v = bitmap_data[p] + err[k + 1];
        if (v > 128) {
          error = (v - 255);
          set_pixel_o_bottom(i, j, 0);
        }
        else {
          error = v;
          set_pixel_o_bottom(i, j, 1);
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

void 
Screen::draw_rectangle(
  uint16_t width, 
  uint16_t height, 
  int16_t x, 
  int16_t y,
  uint8_t color) //, bool show)
{
  uint32_t x_max = x + width;
  uint32_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  if (orientation == O_LEFT) {
    for (int i = x; i < x_max; i++) {
      set_pixel_o_left(i,         y, color);
      set_pixel_o_left(i, y_max - 1, color);
    }
    for (int j = y; j < y_max; j++) {
      set_pixel_o_left(    x,     j, color);
      set_pixel_o_left(x_max - 1, j, color);
    }
  }
  else if (orientation == O_RIGHT) {
    for (int i = x; i < x_max; i++) {
      set_pixel_o_right(i,         y, color);
      set_pixel_o_right(i, y_max - 1, color);
    }
    for (int j = y; j < y_max; j++) {
      set_pixel_o_right(    x,     j, color);
      set_pixel_o_right(x_max - 1, j, color);
    }
  }
  else {
    for (int i = x; i < x_max; i++) {
      set_pixel_o_bottom(i,         y, color);
      set_pixel_o_bottom(i, y_max - 1, color);
    }
    for (int j = y; j < y_max; j++) {
      set_pixel_o_bottom(    x,     j, color);
      set_pixel_o_bottom(x_max - 1, j, color);
    }
  }
}

void
Screen::colorize_region(
  uint16_t width, 
  uint16_t height, 
  int16_t  x, 
  int16_t  y,
  uint8_t  color) //, bool show)
{
  int16_t x_max = x + width;
  int16_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  if (orientation == O_LEFT) {
    for (int j = y; j < y_max; j++) {
      for (int i = x; i < x_max; i++) {
        set_pixel_o_left(i, j, color);
      }
    }
  }
  else if (orientation == O_RIGHT) {
    for (int j = y; j < y_max; j++) {
      for (int i = x; i < x_max; i++) {
        set_pixel_o_right(i, j, color);
      }
    }
  }
  else {
    for (int j = y; j < y_max; j++) {
      for (int i = x; i < x_max; i++) {
        set_pixel_o_bottom(i, j, color);
      }
    }
  }
}

void 
Screen::draw_glyph(
  const unsigned char * bitmap_data, 
  uint16_t width, 
  uint16_t height, 
  uint16_t pitch,
  int16_t x, 
  int16_t y) //, bool show)
{
  int x_max = x + width;
  int y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  if (orientation == O_LEFT) {
    for (uint32_t j = y, q = 0; j < y_max; j++, q++) {  // row
      for (uint32_t i = x, p = (q * pitch) << 3; i < x_max; i++, p++) { // column
        uint8_t v = bitmap_data[p >> 3] & LUT1BIT[p & 7];
        set_pixel_o_left(i, j, v ? 1 : 0);
      }
    }
  }
  else if (orientation == O_RIGHT) {
    for (uint32_t j = y, q = 0; j < y_max; j++, q++) {  // row
      for (uint32_t i = x, p = (q * pitch) << 3; i < x_max; i++, p++) { // column
        uint8_t v = bitmap_data[p >> 3] & LUT1BIT[p & 7];
        set_pixel_o_right(i, j, v ? 1 : 0);
      }
    }
  }
  else {
    for (uint32_t j = y, q = 0; j < y_max; j++, q++) {  // row
      for (uint32_t i = x, p = (q * pitch) << 3; i < x_max; i++, p++) { // column
        uint8_t v = bitmap_data[p >> 3] & LUT1BIT[p & 7];
        set_pixel_o_bottom(i, j, v ? 1 : 0);
      }
    }
  }
}

void Screen::setup()
{
  frame_buffer = (EInk::Bitmap1Bit *) ESP::ps_malloc(sizeof(EInk::Bitmap1Bit));
  clear();
  e_ink.clean();
}