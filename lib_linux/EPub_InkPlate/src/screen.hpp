// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"
#include "picture.hpp"

#include "non_copyable.hpp"

#include <gtk/gtk.h>
#include <memory>

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

class Screen : NonCopyable {
  public:
    static constexpr int8_t IDENT        = 99;
    static constexpr uint16_t RESOLUTION = 166; ///< Pixels per inch

    static std::unique_ptr<guchar[]> pixels;

    enum class Orientation : int8_t {
      LEFT, RIGHT, TOP, BOTTOM
    };
    enum class PixelResolution : int8_t {
      ONE_BIT, THREE_BITS
    };
    enum Color {
      WHITE = 0xFF, BLACK = 0
    };

    auto drawPicture(PicturePtr &picture, Pos pos) -> void;
    auto drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch) -> void;
    auto drawRectangle(Dim dim, Pos pos, Color color) -> void;
    auto drawRoundRectangle(Dim dim, Pos pos, Color color) -> void;
    auto colorizeRegion(Dim dim, Pos pos, Color color) -> void;
    auto clear() -> void;
    auto update(bool noFull = false) -> void; // Parameter only used by the InkPlate version
    auto test() -> void;

  private:
    static constexpr char const *TAG = "Screen";

    static const uint8_t LUT1BIT[8];

    static Screen singleton;

    Screen()  = default;
    ~Screen() = default;

    static uint16_t width;
    static uint16_t height;

    struct PictureData {
      GtkImage *picture;
      int rows, cols, stride;
    };

    PictureData pictureData;
    PixelResolution pixelResolution;
    Orientation orientation;

    enum class Corner : uint8_t {
      TOP_LEFT, TOP_RIGHT, LOWER_LEFT, LOWER_RIGHT
    };
    auto drawArc(uint16_t xMid, uint16_t yMid, uint8_t radius, Corner corner, Color color) -> void;

  public:
    static auto getSingleton() noexcept -> Screen & { return singleton; }
    auto setup(PixelResolution resolution, Orientation orientation) -> void;
    auto setPixelResolution(PixelResolution resolution, bool force = false) -> void;
    auto setOrientation(Orientation orient) -> void;
    auto getOrientation() -> Orientation { return orientation; }
    [[nodiscard]] inline auto getPixelResolution() -> PixelResolution { return pixelResolution; }
    auto getPicture() -> GtkImage * { return pictureData.picture; }
    // auto to_user_coord(uint16_t &x, uint16_t &y) -> void {}
    inline auto forceFullUpdate() -> void {}

    [[nodiscard]] inline static auto getWidth() -> uint16_t { return width; }
    [[nodiscard]] inline static auto getHeight() -> uint16_t { return height; }

    #if TOUCH_TRIAL
      GtkWidget *window, *pictureBox;
    #else
      GtkWidget *window, *leftButton, *rightButton, *upButton, *downButton, *selectButton,
                *homeButton;
    #endif
};

#if __SCREEN__
  Screen &screen = Screen::getSingleton();
#else
  extern Screen &screen;
#endif
