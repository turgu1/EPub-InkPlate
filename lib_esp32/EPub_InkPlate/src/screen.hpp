#ifndef __SCREEN_HPP__
#define __SCREEN_HPP__

#include "noncopyable.hpp"
#include "eink.hpp"

/**
 * @brief Low level logical Screen display
 * 
 * This class implements the low level methods required to paint
 * on the display. Under the InlPlate6, it is using the EInk display driver. 
 * 
 * This is a singleton. It cannot be instanciated elsewhere. It is not 
 * instanciated in the heap. This is reinforced by the C++ construction
 * below. It also cannot be copied through the NonCopyable derivation.
 */

class Screen : NonCopyable
{
  public:
    static const uint16_t WIDTH      = EInk::HEIGHT;
    static const uint16_t HEIGHT     = EInk::WIDTH;
    static const uint16_t RESOLUTION = 166;  ///< Pixels per inch
    
    static const uint8_t  grayscaleLevelCount = 8;

    void put_bitmap(const unsigned char * bitmap_data, 
                    uint16_t width, uint16_t height, 
                    int16_t x, int16_t y);
    void put_bitmap_invert(const unsigned char * bitmap_data, 
                    uint16_t width, uint16_t height, 
                    int16_t x, int16_t y);
    void put_highlight(uint16_t width, uint16_t height, 
                       int16_t x, int16_t y);
    void clear_region(uint16_t width, uint16_t height, 
                      int16_t x, int16_t y);

    inline void clear()  { EInk::clear_bitmap(*frame_buffer); }
    inline void update() { e_ink.clean(); e_ink.update(*frame_buffer); }

  private:
    static constexpr char const * TAG = "Screen";

    static Screen singleton;
    Screen() { };

    EInk::Bitmap3Bit * frame_buffer;

    inline void set_pixel(uint16_t col, uint16_t row, uint8_t color) {
      uint8_t * temp = &(*frame_buffer)[EInk::BITMAP_SIZE_3BIT - (EInk::LINE_SIZE_3BIT * col) + (row >> 1)];
      *temp = col & 1 ? (*temp & 0xF0) | color : (*temp & 0x0F) | (color << 4);
    }

  public:
    static Screen & get_singleton() noexcept { return singleton; }
    void setup();
};

#if __SCREEN__
  Screen & screen = Screen::get_singleton();
#else
  extern Screen & screen;
#endif

#endif