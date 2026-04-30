// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test/stubs/screen.hpp — headless stub that replaces lib_linux/EPub_InkPlate/src/screen.hpp
//
// The real screen.hpp includes <gtk/gtk.h> which is not available in the
// headless test environment.  This stub provides just enough of the Screen
// interface to let epub.cpp compile and link: the IDENT constant and the
// width/height accessors used by updateBookFormatParams() and getPicture().
//
// The no-op drawing methods (drawGlyph, drawPicture, etc.) are needed by the
// Valgrind S5 build which compiles html_interpreter.cpp and page.cpp.  They
// are safe to include here because Page::paint() only calls them when
// computeMode == DISPLAY, and pageLocs computation always uses LOCATION mode.
// ---------------------------------------------------------------------------

#pragma once
#include "global.hpp"
#include "non_copyable.hpp"
#include "picture.hpp" // for PicturePtr used in drawPicture signature

class Screen : NonCopyable {
public:
  /// Device identity constant (must match the value in the real screen.hpp).
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

  [[nodiscard]] static auto getWidth() -> uint16_t { return 600; }
  [[nodiscard]] static auto getHeight() -> uint16_t { return 800; }
  [[nodiscard]] auto getPixelResolution() const -> PixelResolution {
    return PixelResolution::THREE_BITS;
  }

  // No-op stubs for layout/display methods — never called in LOCATION compute mode.
  auto drawPicture(PicturePtr &, Pos) -> void {}
  auto drawGlyph(const unsigned char *, Dim, Pos, uint16_t) -> void {}
  auto drawRectangle(Dim, Pos, Color) -> void {}
  auto drawRoundRectangle(Dim, Pos, Color) -> void {}
  auto colorizeRegion(Dim, Pos, Color) -> void {}
  auto setup(PixelResolution, Orientation) -> void {}
  auto update(bool = false) -> void {}
  auto clear() -> void {}
  auto test() -> void {}

  Screen() = default;
};

// screen global — defined in exactly one translation unit by #define __SCREEN__ 1
#if __SCREEN__
  static Screen s_screen_instance;
  Screen &screen = s_screen_instance;
#else
  extern Screen &screen;
#endif
