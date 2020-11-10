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

void 
Screen::put_bitmap(
  const unsigned char * bitmap_data, 
  uint16_t width, 
  uint16_t height, 
  int16_t x, 
  int16_t y) //, bool show)
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

  for (uint32_t j = y, q = 0; j < y_max; j++, q++) {  // rows
    for (uint32_t i = x, p = q * width; i < x_max; i++, p++) {  // columns
      uint8_t v = bitmap_data[p];
      if (v != 255) { // Do not paint white pixels
        set_pixel(i, j, v >> 5);
      }
    }
  }
}

void 
Screen::set_region(
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

  for (uint16_t j = y; j < y_max; j++) {
    for (uint16_t i = x; i < x_max; i++) {
      set_pixel(i, j, color);
    }
  }
}

void 
Screen::put_bitmap_invert(
  const unsigned char * bitmap_data, 
  uint16_t width, 
  uint16_t height, 
  int16_t x, 
  int16_t y) //, bool show)
{
  int x_max = x + width;
  int y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  for (uint32_t j = y, q = 0; j < y_max; j++, q++) {
    for (uint32_t i = x, p = q * width; i < x_max; i++, p++) {
      uint8_t v = (255 - bitmap_data[p]);
      if (v != 255) {
        set_pixel(i, j, v >> 5);
      }
    }
  }
}

void Screen::setup()
{
  frame_buffer = (EInk::Bitmap3Bit *) ESP::ps_malloc(sizeof(EInk::Bitmap3Bit));
  clear();
}