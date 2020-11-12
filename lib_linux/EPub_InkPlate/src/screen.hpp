#ifndef __SCREEN_HPP__
#define __SCREEN_HPP__

#include "noncopyable.hpp"

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
    static const uint16_t WIDTH           = 600;
    static const uint16_t HEIGHT          = 800;
    static const uint16_t RESOLUTION      = 166;  ///< Pixels per inch
    static const uint8_t  HIGHLIGHT_COLOR = 0x00;
    static const uint8_t  WHITE_COLOR     = 0xFF;
    
    void draw_bitmap(const unsigned char * bitmap_data, 
                    uint16_t width, uint16_t height, 
                    int16_t x, int16_t y);
    void draw_glyph(const unsigned char * bitmap_data, 
                    uint16_t width, uint16_t height, uint16_t pitch,
                    int16_t x, int16_t y);
    void draw_rectangle(uint16_t width, uint16_t height, 
                        int16_t x, int16_t y,
                        uint8_t color);
    void  clear();
    void update();
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

  public:
    static Screen & get_singleton() noexcept { return singleton; }
    void setup();
    
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