/*
Graphics.cpp
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

#include "Graphics.hpp"
#include "esp.hpp"
#include "eink.hpp"

#include "logging.hpp"

static const char * TAG = "Graphics";

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                                                            \
    {                                                                                                                  \
        int16_t t = a;                                                                                                 \
        a = b;                                                                                                         \
        b = t;                                                                                                         \
    }
#endif

Graphics::Graphics()
{
  _partial     = (EInk::Bitmap1Bit *) ESP::ps_malloc(sizeof(EInk::Bitmap1Bit));
  D_memory4Bit = (EInk::Bitmap3Bit *) ESP::ps_malloc(sizeof(EInk::Bitmap3Bit));

  if ((_partial == nullptr) || (D_memory4Bit == nullptr)) {
    ESP_LOGE(TAG, "Unable to allocate PSRAM memory for buffers.");
  }

  ESP_LOGD(TAG, "Buffers addresses: _partial 0x%08x, D_memory4Bit 0x%08x", (int)_partial, (int)D_memory4Bit);
  EInk::clear_bitmap(*_partial);
  EInk::clear_bitmap(*D_memory4Bit);

  _width  = EInk::WIDTH;
  _height = EInk::HEIGHT;

  _displayMode = -1;
}

void Graphics::show()
{
  ESP_LOGD(TAG, "Display Mode: %d.", getDisplayMode());
  
  if (getDisplayMode() == 1) {
    e_ink.update(*_partial);
  }
  else {
    e_ink.update(*D_memory4Bit);
  }
}

void Graphics::setRotation(uint8_t x)
{
    rotation = (x & 3);
    switch (rotation)
    {
    case 0:
    case 2:
        _width  = EInk::WIDTH;
        _height = EInk::HEIGHT;
        break;
    case 1:
    case 3:
        _width  = EInk::HEIGHT;
        _height = EInk::WIDTH;
        break;
    }
}

uint8_t Graphics::getRotation()
{
    return rotation;
}

void Graphics::drawPixel(int16_t x0, int16_t y0, uint16_t color)
{
    writePixel(x0, y0, color);
}

void Graphics::startWrite()
{
}

void Graphics::writePixel(int16_t x0, int16_t y0, uint16_t color)
{    
    if (x0 > width() - 1 || y0 > height() - 1 || x0 < 0 || y0 < 0)
        return;

    switch (rotation)
    {
    case 1:
        _swap_int16_t(x0, y0);
        x0 = height() - x0 - 1;
        break;
    case 2:
        x0 = width() - x0 - 1;
        y0 = height() - y0 - 1;
        break;
    case 3:
        _swap_int16_t(x0, y0);
        y0 = width() - y0 - 1;
        break;
    }

    //ESP_LOGD(TAG, "Display mode = %d", getDisplayMode());

    if (getDisplayMode() == 1)
    {
      int x = x0 >> 3;
      int x_sub = x0 & 7;
      uint8_t * temp = &(*_partial)[100 * y0 + x];
      //ESP_LOGD(TAG, "writePixel [%d, %d] (%08x) : %d", x0, y0, (int) temp, color);
      *temp = (~pixelMaskLUT[x_sub] & *temp) | (color ? pixelMaskLUT[x_sub] : 0);
    }
    else if (getDisplayMode() == 3)
    {
        color &= 7;
        int x = x0 >> 1;
        int x_sub = x0 & 1;
        uint8_t * temp = &(*D_memory4Bit)[400 * y0 + x];
        *temp = (pixelMaskGLUT[x_sub] & *temp) | (x_sub ? color : color << 4);
    }
}

void Graphics::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    for (int i = 0; i < h; ++i)
        for (int j = 0; j < w; ++j)
            writePixel(x + j, y + i, color);
}

void Graphics::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    for (int i = 0; i < h; ++i)
        writePixel(x, y + i, color);
}

void Graphics::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    for (int j = 0; j < w; ++j)
        writePixel(x + j, y, color);
}

void Graphics::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        _swap_int16_t(x0, y0);
        _swap_int16_t(x1, y1);
    }

    if (x0 > x1)
    {
        _swap_int16_t(x0, x1);
        _swap_int16_t(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx >> 1;
    int16_t ystep;

    if (y0 < y1)
        ystep = 1;
    else
        ystep = -1;

    for (; x0 <= x1; x0++)
    {
        if (steep)
            writePixel(y0, x0, color);
        else
            writePixel(x0, y0, color);
        err -= dy;
        if (err < 0)
        {
            y0 += ystep;
            err += dx;
        }
    }
}

void Graphics::endWrite()
{
}

void Graphics::setDisplayMode(uint8_t _mode)
{
    _displayMode = _mode;
}

void Graphics::selectDisplayMode(uint8_t _mode)
{
    if (_mode != _displayMode)
    {
        _displayMode = _mode;
        if (_mode == 1) {
          EInk::clear_bitmap(*_partial);
        }
        else {
          EInk::clear_bitmap(*D_memory4Bit);
        }
        // memset(_partial, 0, 60000);
        // memset(D_memory4Bit, 255, 240000);
        //_blockPartial = 1;
    }
}

uint8_t Graphics::getDisplayMode()
{
    return _displayMode;
}

int16_t Graphics::width()
{
    return _width;
};

int16_t Graphics::height()
{
    return _height;
};