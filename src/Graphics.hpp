/*
Graphics.h
Inkplate 6 Arduino library
David Zovko, Borna Biro, Denis Vajak, Zvonimir Haramustek @ e-radionica.com
September 24, 2020
https://github.com/e-radionicacom/Inkplate-6-Arduino-library

For support, please reach over forums: forum.e-radionica.com/en
For more info about the product, please check: www.inkplate.io

This code is released under the GNU Lesser General Public License v3.0: https://www.gnu.org/licenses/lgpl-3.0.en.html
Please review the LICENSE file included with this example.
If you have any questions about licensing, please contact techsupport@e-radionica.com
Distributed as-is; no warranty is given.
*/

#include "eink.hpp"

#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif

#if !defined(__INT_MAX__) || (__INT_MAX__ > 0xFFFF)
#define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))
#else
#define pgm_read_pointer(addr) ((void *)pgm_read_word(addr))
#endif

class Graphics
{
  public:
    Graphics();

    void    show();
    void    setRotation(uint8_t r);
    uint8_t getRotation();

    void drawPixel(int16_t x, int16_t y, uint16_t color);

    void selectDisplayMode(uint8_t _mode);

    void    setDisplayMode(uint8_t _mode);
    uint8_t getDisplayMode();
    int16_t width();
    int16_t height();

    EInk::Bitmap1Bit * _partial;
    EInk::Bitmap3Bit * D_memory4Bit;

    const uint8_t pixelMaskLUT[8] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
    const uint8_t pixelMaskGLUT[2] = {0xF, 0xF0};

    void startWrite(void);
    void writePixel(int16_t x, int16_t y, uint16_t color);
    void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void endWrite(void);

  private:
    uint8_t rotation;
    uint16_t _width, _height;

    uint8_t _displayMode = -1;

  protected:
};

#endif