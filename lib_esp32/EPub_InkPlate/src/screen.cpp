// Using GTK on Linux to emulate InkPlate screen
//
// As all GTK related code is located in this module, we also implement
// some part of the event manager code here...


#define __SCREEN__ 1
#include "screen.hpp"
#include "esp.hpp"

#include <iomanip>

#define BYTES_PER_PIXEL 3

//static const char * TAG = "Screen";

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

  uint16_t x_max = x + width;
  uint16_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  for (uint16_t j = y, q = 0; j < y_max; j++, q++) {  // rows
    for (uint16_t i = x, p = q * width; i < x_max; i++) {  // columns
      uint8_t v = bitmap_data[p];
      if (v != 255) { // Do not paint white pixels
        set_pixel(i, j, v >> 5);
      }
      p++;
    }
  }
}

void 
Screen::put_highlight(
  uint16_t width, 
  uint16_t height, 
  int16_t x, 
  int16_t y) //, bool show)
{
  uint16_t x_max = x + width;
  uint16_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  for (uint16_t j = y, q = 0; j < y_max; j++, q++) {
    for (uint16_t i = x, p = q * width; i < x_max; i++, p++) {
      set_pixel(i, j, 6);
    }
  }
}

void 
Screen::clear_region(
  uint16_t width, 
  uint16_t height, 
  int16_t x, 
  int16_t y) //, bool show)
{
  uint16_t x_max = x + width;
  uint16_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  for (uint16_t j = y, q = 0; j < y_max; j++, q++) {
    for (uint16_t i = x, p = q * width; i < x_max; i++, p++) {
      set_pixel(i, j, 7);
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

  for (int j = y, q = 0; j < y_max; j++, q++) {
    for (int i = x, p = q * width; i < x_max; i++) {
      uint8_t v = (255 - bitmap_data[p]);
      if (v != 255) {
        set_pixel(i, j, v >> 5);
      }
      p++;
    }
  }
}

void Screen::setup()
{
  frame_buffer = (EInk::Bitmap3Bit *) ESP::ps_malloc(sizeof(EInk::Bitmap3Bit));
  clear();
}