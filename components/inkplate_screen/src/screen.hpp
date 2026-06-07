// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "inkplate_platform.hpp"
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
    #if INKPLATE_10 || INKPLATE_10_V2
      static constexpr int8_t IDENT                 = 2;
      static constexpr int8_t PARTIAL_COUNT_ALLOWED = 10;
      static constexpr uint16_t RESOLUTION          = 150;///< Pixels per inch
    #elif INKPLATE_6 || INKPLATE_5_V2 || INKPLATE_6_V2
      static constexpr int8_t IDENT                 = 1;
      static constexpr int8_t PARTIAL_COUNT_ALLOWED = 10;
      static constexpr uint16_t RESOLUTION          = 166;///< Pixels per inch
    #elif INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
      static constexpr int8_t IDENT                  = 3;
      static constexpr int16_t PARTIAL_COUNT_ALLOWED = 10;
      static constexpr uint16_t RESOLUTION           = 212;///< Pixels per inch
    #endif
    enum class Orientation : int8_t { LEFT, RIGHT, BOTTOM, TOP };
    enum class PixelResolution : int8_t { ONE_BIT, THREE_BITS };

    enum Color { WHITE = 0, BLACK = 7 };

    auto drawPicture(PicturePtr &picture, Pos pos) -> void;
    auto drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch) -> void;
    auto drawRectangle(Dim dim, Pos pos, uint8_t color) -> void;
    auto drawRoundRectangle(Dim dim, Pos pos, uint8_t color) -> void;
    auto colorizeRegion(Dim dim, Pos pos, uint8_t color) -> void;

    auto lowColorize1Bit(Dim dim, Pos pos, uint8_t color) -> void;
    auto lowColorize3Bit(Dim dim, Pos pos, uint8_t color) -> void;

    inline auto clear() -> void {
      if (pixelResolution == PixelResolution::ONE_BIT) {
        frameBuffer1Bit->clear();
      } else {
        frameBuffer3Bit->clear();
      }
    }

    inline auto update(bool noFull = false) -> void {
      if (pixelResolution == PixelResolution::ONE_BIT) {
        if (noFull) {
          e_ink.partial_update(*frameBuffer1Bit);
          partialCount = 0;
        } else {
          if (partialCount <= 0) {
            // e_ink.clean();
            e_ink.update(*frameBuffer1Bit);
            partialCount = PARTIAL_COUNT_ALLOWED;
          } else {
            e_ink.partial_update(*frameBuffer1Bit);
            partialCount--;
          }
        }
      } else {
        e_ink.update(*frameBuffer3Bit);
      }
    }

  private:
    static constexpr char const *TAG = "Screen";

    static const uint8_t LUT1BIT[8];
    static const uint8_t LUT1BIT_INV[8];

    static uint16_t width;
    static uint16_t height;
    static uint16_t yOffset;
    ;

    static Screen singleton;

    int16_t partialCount{ 0 };
    FrameBuffer1Bit *frameBuffer1Bit{ nullptr };
    FrameBuffer3Bit *frameBuffer3Bit{ nullptr };
    PixelResolution pixelResolution{ PixelResolution::ONE_BIT };
    Orientation orientation{ Orientation::LEFT };

    enum class Corner : uint8_t { TOP_LEFT, TOP_RIGHT, LOWER_LEFT, LOWER_RIGHT };

    Screen() = default;

    auto drawArc(uint16_t xMid, uint16_t yMid, uint8_t radius, Corner corner, uint8_t color) -> void;

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

    inline auto setPixelOLeft1Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
      uint8_t *temp =
        &(frameBuffer1Bit->get_data())[frameBuffer1Bit->get_data_size() -
                                       (frameBuffer1Bit->get_line_size() * (col + 1)) + (row >> 3)];
      if (color == 1) {
        *temp = *temp | LUT1BIT_INV[row & 7];
      }
      else {
        *temp = (*temp & ~LUT1BIT_INV[row & 7]);
      }
    }

    inline auto setPixelORight1Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
      uint8_t *temp =
        &(frameBuffer1Bit
          ->get_data())[(frameBuffer1Bit->get_line_size() * (col + 1)) - (row >> 3) - 1];
      if (color == 1) {
        *temp = *temp | LUT1BIT[row & 7];
      }
      else {
        *temp = (*temp & ~LUT1BIT[row & 7]);
      }
    }

    inline auto setPixelOBottom1Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
      uint8_t *temp =
        &(frameBuffer1Bit->get_data())[frameBuffer1Bit->get_line_size() * row + (col >> 3)];
      if (color == 1) {
        *temp = *temp | LUT1BIT_INV[col & 7];
      }
      else {
        *temp = (*temp & ~LUT1BIT_INV[col & 7]);
      }
    }

    inline auto setPixelOTop1Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
      uint8_t *temp = &(frameBuffer1Bit->get_data())[frameBuffer1Bit->get_data_size() -
                                                     (frameBuffer1Bit->get_line_size() * row) -
                                                     ((col + yOffset) >> 3)];
      if (color == 1) {
        *temp = *temp | LUT1BIT[(col + yOffset) & 7];
      }
      else {
        *temp = (*temp & ~LUT1BIT[(col + yOffset) & 7]);
      }
    }

    inline auto setPixelOLeft3Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
      uint8_t *temp =
        &(frameBuffer3Bit->get_data())[frameBuffer3Bit->get_data_size() -
                                       (frameBuffer3Bit->get_line_size() * (col + 1)) + (row >> 1)];
      if (row & 1) {
        *temp = (*temp & 0xF0) | color;
      }
      else {
        *temp = (*temp & 0x0F) | (color << 4);
      }
    }

    inline auto setPixelORight3Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
      uint8_t *temp =
        &(frameBuffer3Bit
          ->get_data())[(frameBuffer3Bit->get_line_size() * (col + 1)) - (row >> 1) - 1];
      if (row & 1) {
        *temp = (*temp & 0x0F) | (color << 4);
      }
      else {
        *temp = (*temp & 0xF0) | color;
      }
    }

    inline auto setPixelOBottom3Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
      uint8_t *temp =
        &(frameBuffer3Bit->get_data())[frameBuffer3Bit->get_line_size() * row + (col >> 1)];
      if (col & 1) {
        *temp = (*temp & 0xF0) | color;
      }
      else {
        *temp = (*temp & 0x0F) | (color << 4);
      }
    }

    inline auto setPixelOTop3Bit(uint32_t col, uint32_t row, uint8_t color) -> void {
      uint8_t *temp =
        &(frameBuffer3Bit->get_data())[frameBuffer3Bit->get_data_size() -
                                       (frameBuffer3Bit->get_line_size() * row) - ((col + yOffset) >> 1)];
      if ((col + yOffset) & 1) {
        *temp = (*temp & 0x0F) | (color << 4);
      }
      else {
        *temp = (*temp & 0xF0) | color;
      }
    }

    auto setYOffset() -> void;

  public:
    static Screen &getSingleton() noexcept { return singleton; }
    auto setup(PixelResolution resolution, Orientation orientation) -> void;
    auto setPixelResolution(PixelResolution resolution, bool force = false) -> void;
    auto setOrientation(Orientation orient) -> void;
    [[nodiscard]] inline auto getOrientation() -> Orientation { return orientation; }
    [[nodiscard]] inline auto getPixelResolution() -> PixelResolution { return pixelResolution; }
    inline auto forceFullUpdate() -> void { partialCount = 0; }

    // #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    //   void to_user_coord(uint16_t &x, uint16_t &y);
    // #endif

    [[nodiscard]] inline static auto getWidth() -> uint16_t { return width; }
    [[nodiscard]] inline static auto getHeight() -> uint16_t { return height; }
    [[nodiscard]] inline static auto getYOffset() -> uint16_t { return yOffset; }
};

#if __SCREEN__
  Screen &screen = Screen::getSingleton();
#else
  extern Screen &screen;
#endif
