// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"
#include "picture.hpp"

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

class Screen : NonCopyable {
public:
  static constexpr int8_t IDENT        = 99;
  static constexpr uint16_t RESOLUTION = 166; ///< Pixels per inch

  enum class Orientation : int8_t {
    LEFT, RIGHT, BOTTOM
  };
  enum class PixelResolution : int8_t {
    ONE_BIT, THREE_BITS
  };
  enum Color {
    WHITE = 0xFF, BLACK = 0
  };

  void drawPicture(PicturePtr &picture, Pos pos);
  void drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch);
  void drawRectangle(Dim dim, Pos pos, Color color);
  void drawRoundRectangle(Dim dim, Pos pos, Color color);
  void colorizeRegion(Dim dim, Pos pos, Color color);
  void clear();
  void update(bool noFull = false); // Parameter only used by the InkPlate version
  void test();

private:
  static constexpr char const *TAG = "Screen";

  static const uint8_t LUT1BIT[8];

  static Screen singleton;
  Screen() {};

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
  void drawArc(uint16_t xMid, uint16_t yMid, uint8_t radius, Corner corner, Color color);

public:
  static Screen &getSingleton() noexcept { return singleton; }
  void setup(PixelResolution resolution, Orientation orientation);
  void setPixelResolution(PixelResolution resolution, bool force = false);
  void setOrientation(Orientation orient);
  inline PixelResolution getPixelResolution() { return pixelResolution; }
  GtkImage *getPicture() { return pictureData.picture; }
  // void to_user_coord(uint16_t &x, uint16_t &y) {}
  inline void forceFullUpdate() {}

  inline static uint16_t getWidth() { return width; }
  inline static uint16_t getHeight() { return height; }

  #if TOUCH_TRIAL
    GtkWidget *window, *picture_box;
  #else
    GtkWidget *window, *left_button, *right_button, *up_button, *down_button, *select_button,
        *home_button;
  #endif
};

#if __SCREEN__
  Screen &screen = Screen::getSingleton();
#else
  extern Screen &screen;
#endif
