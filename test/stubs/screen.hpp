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
// ---------------------------------------------------------------------------

#pragma once
#include "global.hpp"

class Screen {
public:
  /// Device identity constant (must match the value in the real screen.hpp).
  static constexpr int8_t  IDENT      = 99;
  static constexpr uint16_t RESOLUTION = 166; ///< Pixels per inch

  enum class Orientation     : int8_t { LEFT, RIGHT, BOTTOM };
  enum class PixelResolution : int8_t { ONE_BIT, THREE_BITS };
  enum Color { WHITE = 0xFF, BLACK = 0 };

  [[nodiscard]] static auto getWidth()  -> uint16_t { return 600; }
  [[nodiscard]] static auto getHeight() -> uint16_t { return 800; }

  // No-op stubs for methods referenced by code compiled alongside epub.cpp.
  auto setup(PixelResolution, Orientation) -> void {}
  auto update(bool = false) -> void {}
  auto clear() -> void {}
};

#if __SCREEN__
  Screen &screen = *reinterpret_cast<Screen *>(nullptr); // never actually used in tests
#else
  // extern Screen &screen; -- not needed for epub tests
#endif
