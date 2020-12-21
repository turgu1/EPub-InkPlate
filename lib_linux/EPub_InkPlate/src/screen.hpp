// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __SCREEN_HPP__
#define __SCREEN_HPP__

#include "global.hpp"
#include "noncopyable.hpp"

#include <cinttypes>

#include <gtk/gtk.h>

/**
 * @brief Low level logical Screen display
 * 
 * This class implements the low level methods required to paint
 * on the display. Under Linux, it generate a GTK window. On a InkPlate6
 * it is using the EInk display driver. 
 * 
 * This is a singleton. It cannot be instanciated elsewhere. It is not 
 * instanciated in the heap. This is reinforced by the C++ construction
 * below. It also cannot be copied through the NonCopyable derivation.
 */

class Screen : NonCopyable
{
  public:
    static uint16_t WIDTH;
    static uint16_t HEIGHT;
    static const uint16_t RESOLUTION  =  166;  ///< Pixels per inch
    static const uint8_t  BLACK_COLOR = 0x00;
    static const uint8_t  WHITE_COLOR = 0xFF;
    
    enum class Orientation     : int8_t { LEFT, RIGHT, BOTTOM };
    enum class PixelResolution : int8_t { ONE_BIT, THREE_BITS };

    void     draw_bitmap(const unsigned char * bitmap_data, Dim dim, Pos pos);
    void      draw_glyph(const unsigned char * bitmap_data, Dim dim, Pos pos, uint16_t pitch);
    void  draw_rectangle(Dim dim, Pos pos, uint8_t color);
    void colorize_region(Dim dim, Pos pos, uint8_t color);
    void  clear();
    void update(bool no_full = false); // Parameter only used by the InlPlate version
    void   test();

  private:
    static constexpr char const * TAG = "Screen";

    static const uint8_t LUT1BIT[8];

    static Screen singleton;
    Screen() {};


    struct ImageData {
      GtkImage * image;
      int rows, cols, stride;
    };
    ImageData id;
    PixelResolution pixel_resolution;
    Orientation     orientation;

  public:
    static Screen & get_singleton() noexcept { return singleton; }
    void setup(PixelResolution resolution, Orientation orientation);
    void set_pixel_resolution(PixelResolution resolution, bool force = false);
    void set_orientation(Orientation orient);
    inline PixelResolution get_pixel_resolution() { return pixel_resolution; }
    
    GtkWidget
      * window, 
      * left_button,
      * right_button,
      * up_button,
      * down_button,
      * select_button,
      * home_button;
};

#if __SCREEN__
  Screen & screen = Screen::get_singleton();
#else
  extern Screen & screen;
#endif

#endif