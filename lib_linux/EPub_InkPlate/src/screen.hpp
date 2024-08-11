// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "non_copyable.hpp"

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
    static constexpr int8_t   IDENT       =   99;
    static constexpr uint16_t RESOLUTION  =  166;  ///< Pixels per inch
    
    enum class Orientation     : int8_t { LEFT, RIGHT, BOTTOM };
    enum class PixelResolution : int8_t { ONE_BIT, THREE_BITS };
    enum Color { WHITE = 0xFF, BLACK = 0 };

    void           draw_bitmap(const unsigned char * bitmap_data, Dim dim, Pos pos);
    void            draw_glyph(const unsigned char * bitmap_data, Dim dim, Pos pos, uint16_t pitch);
    void        draw_rectangle(Dim dim, Pos pos, Color color);
    void  draw_round_rectangle(Dim dim, Pos pos, Color color);
    void       colorize_region(Dim dim, Pos pos, Color color);
    void                 clear();
    void                update(bool no_full = false); // Parameter only used by the InkPlate version
    void                  test();

  private:
    static constexpr char const * TAG = "Screen";

    static const uint8_t LUT1BIT[8];

    static Screen singleton;
    Screen() {};

    static uint16_t width;
    static uint16_t height;

    struct ImageData {
      GtkImage * image;
      int rows, cols, stride;
    };

    ImageData       image_data;
    PixelResolution pixel_resolution;
    Orientation     orientation;

    enum class Corner : uint8_t { TOP_LEFT, TOP_RIGHT, LOWER_LEFT, LOWER_RIGHT };
    void draw_arc(uint16_t x_mid,  uint16_t y_mid,  uint8_t radius, Corner corner, Color color);

  public:
    static Screen &               get_singleton() noexcept { return singleton; }
    void                                  setup(PixelResolution resolution, 
                                                Orientation     orientation);
    void                   set_pixel_resolution(PixelResolution resolution, bool force = false);
    void                        set_orientation(Orientation orient);
    inline PixelResolution get_pixel_resolution() { return pixel_resolution; }
    GtkImage *                        get_image() { return image_data.image; }
    void                          to_user_coord(uint16_t & x, uint16_t & y) {}
    inline void               force_full_update() { }

    inline static uint16_t get_width() { return width; }
    inline static uint16_t get_height() { return height; }
    
    #if TOUCH_TRIAL
      GtkWidget
        * window,
        * image_box;
    #else
      GtkWidget
        * window, 
        * left_button,
        * right_button,
        * up_button,
        * down_button,
        * select_button,
        * home_button;
    #endif
};

#if __SCREEN__
  Screen & screen = Screen::get_singleton();
#else
  extern Screen & screen;
#endif
