// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "platform.hpp"
#include "non_copyable.hpp"
#include "picture.hpp"

/**
 * @brief Low level logical Screen display
 *
 * This class implements the low level methods required to paint
 * on the display. Under the InkPlate6, it is using the EInk display driver.
 *
 * This is a singleton. It cannot be instanciated elsewhere. It is not
 * instanciated in the heap. This is reinforced by the C++ construction
 * below. It also cannot be copied through the NonCopyable derivation.
 */

class Screen : NonCopyable {
  public:
    enum class Orientation : int8_t { LEFT, RIGHT, BOTTOM, TOP };
    enum class PixelResolution : int8_t { MONO, GRAY };

    enum Color { WHITE = 0, BLACK = 0x0F };

    auto drawPicture(PicturePtr &picture, Pos pos) -> void;
    auto drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch, bool inverse = false) -> void;
    auto drawRectangle(Dim dim, Pos pos, uint8_t color, int8_t thickness = 1) -> void;
    auto drawRoundedRectangle(Dim dim, Pos p0, uint8_t color, int8_t thickness = 1, int8_t radius = 8) -> void;
    auto colorizeRegion(Dim dim, Pos pos, uint8_t color) -> void;

    auto lowColorizeMono(Dim dim, Pos pos, uint8_t color) -> void;
    auto lowColorizeGray(Dim dim, Pos pos, uint8_t color) -> void;

    auto clear() -> void {
      if (pixelResolution == PixelResolution::MONO) {
        frameBufferMono->clear();
      } else {
        frameBufferGray->clear();
      }
    }

    auto update(bool noFull = false) -> void {
      if (pixelResolution == PixelResolution::MONO) {
        if (noFull) {
          platform.getEInk().partialUpdate(*frameBufferMono);
          updateCount = 0;
        } else {
          if (updateCount <= 0) {
            // e_ink.clean();
            platform.getEInk().update(*frameBufferMono);
            updateCount = platform.getEInk().getUpdateCountAllowed();
          } else {
            platform.getEInk().partialUpdate(*frameBufferMono);
            updateCount--;
          }
        }
      } else {
        if (noFull) {
          platform.getEInk().update(*frameBufferGray, EInk::GrayUpdateType::SMOOTH);
          updateCount = 0;
        } else {
          if (updateCount <= 0) {
            platform.getEInk().update(*frameBufferGray, EInk::GrayUpdateType::SLOW);
            updateCount = platform.getEInk().getUpdateCountAllowed();
          } else {
            platform.getEInk().update(*frameBufferGray, EInk::GrayUpdateType::SMOOTH);
            updateCount--;
          }
        }
      }
    }

  private:
    static constexpr char const *TAG = "Screen";

    static const uint8_t LUT_MONO[8];
    static const uint8_t LUT_MONO_INV[8];

    static uint16_t width;
    static uint16_t height;
    static uint16_t yOffset;

    static Screen singleton;

    int16_t updateCount{ 0 };
    FrameBufferMono *frameBufferMono{ nullptr };
    FrameBufferGray *frameBufferGray{ nullptr };
    PixelResolution pixelResolution{ PixelResolution::MONO };
    Orientation orientation{ Orientation::LEFT };

    enum class Corner : uint8_t { TOP_LEFT, TOP_RIGHT, LOWER_LEFT, LOWER_RIGHT };

    Screen() = default;

    auto drawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint8_t color) -> void;
    // auto drawArc(uint16_t xMid, uint16_t yMid, uint8_t radius, Corner corner, uint8_t color) -> void;

    // The Top, Left, Right and Bottom indicators are related to the first E-Radionica
    // Inkplate-6 device, where the 3 touch buttons are located. The Bottom location is the
    // most natural location considering the screen addressing of lines and columns.
    //
    // For the Inkplate-6, there is 600 lines (height) of 800 pixels (width) on screen.
    //
    // The col and row parameters are related to the current screen rotation requested by the
    // user. The Following methods are translating the col and row to retrieve the proper byte
    // location where a pixel needs to be set.
    //

    auto setPixel(Pos pos, uint8_t color) -> void;

    auto   setPixelLeftMono(Pos pos, uint8_t color) -> void;
    auto  setPixelRightMono(Pos pos, uint8_t color) -> void;
    auto setPixelBottomMono(Pos pos, uint8_t color) -> void;
    auto    setPixelTopMono(Pos pos, uint8_t color) -> void;
    auto   setPixelLeftGray(Pos pos, uint8_t color) -> void;
    auto  setPixelRightGray(Pos pos, uint8_t color) -> void;
    auto setPixelBottomGray(Pos pos, uint8_t color) -> void;
    auto    setPixelTopGray(Pos pos, uint8_t color) -> void;

    auto setYOffset() -> void;

  public:
    static Screen &getSingleton() noexcept { return singleton; }
    auto setup(PixelResolution resolution, Orientation orientation) -> void;
    auto setPixelResolution(PixelResolution resolution, bool force = false) -> void;
    auto setOrientation(Orientation orient) -> void;
    auto getOrientation() const -> Orientation { return orientation; }
    auto getPixelResolution() const -> PixelResolution { return pixelResolution; }
    auto getPixelsPerInch() const -> uint16_t { return platform.getEInk().getPixelsPerInch(); }
    auto getIdent() const -> uint8_t { return platform.getEInk().getIdent(); }
    auto forceFullUpdate() -> void { updateCount = 0; }

    // #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    //   void to_user_coord(uint16_t &x, uint16_t &y);
    // #endif

    static auto getWidth() -> uint16_t { return width; }
    static auto getHeight() -> uint16_t { return height; }
    static auto getYOffset() -> uint16_t { return yOffset; }
};

#if __SCREEN__
  Screen &screen = Screen::getSingleton();
#else
  extern Screen &screen;
#endif
