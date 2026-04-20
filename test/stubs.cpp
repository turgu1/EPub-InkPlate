// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test/stubs.cpp — minimal stubs for symbols required by the test build
//                  that live in hardware/viewer code excluded from the tests.
//
// Stubs provided here:
//   • Fonts class + global      — avoids fonts.cpp / FreeType dependencies
//   • MsgViewer::outOfMemory()  — abort instead of hardware shutdown
//   • MsgViewer::show()         — no-op, returns nullptr
//   • DisplayList::getNewEntry()— avoids pulling in the real msg_viewer.hpp
//   • TOC::loadFromEpub()       — no-op (pageLocsInstance is false in tests)
//   • JPegPicture constructor   — no-op (getPicture() never called in tests)
//   • PngPicture constructor    — no-op (getPicture() never called in tests)
// ---------------------------------------------------------------------------

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// Fonts  — MUST be defined before any header that includes models/fonts.hpp
//           (display_list.hpp → models/fonts.hpp via the included chain, so
//            we must emit the definition before including display_list.hpp).
// ============================================================================

#define __FONTS__ 1          // makes fonts.hpp emit `Fonts fonts;`
#include "models/fonts.hpp"  // includes char_pool.hpp, models/font.hpp — no GTK

Fonts::Fonts()  {}
Fonts::~Fonts() {}

auto Fonts::adjustDefaultFont(uint8_t) -> void {}
auto Fonts::clear(bool)                -> void {}
auto Fonts::setup()                    -> bool { return true; }
auto Fonts::clearGlyphCaches()         -> void {}

auto Fonts::add(const std::string &, FaceStyle, MemoryFontPtr, int32_t,
                const std::string &) -> bool { return false; }
auto Fonts::add(const std::string &, FaceStyle,
                const std::string &) -> bool { return false; }
auto Fonts::replace(int16_t, const std::string &, FaceStyle,
                    const std::string &) -> bool { return false; }
auto Fonts::adjustFontStyle(FaceStyle s, FaceStyle, FaceStyle) const -> FaceStyle { return s; }
auto Fonts::getIndex(const std::string &, FaceStyle) -> int16_t { return -1; }

// ============================================================================
// DisplayList::getNewEntry()  (avoids msg_viewer.hpp → screen.hpp → gtk.h)
// ============================================================================

#include "display_list.hpp"

auto DisplayList::getNewEntry() -> DisplayListEntry * {
  DisplayListEntry *entry = entryPool.newElement();
  if (entry == nullptr) {
    std::fprintf(stderr, "[STUB] DisplayList::getNewEntry() — pool exhausted, aborting\n");
    std::abort();
  }
  return entry;
}

// ============================================================================
// MsgViewer stubs
// test/stubs/viewers/msg_viewer.hpp shadows the real header (no GTK needed).
// ============================================================================

#include "viewers/msg_viewer.hpp"  // → test/stubs/viewers/msg_viewer.hpp

auto MsgViewer::outOfMemory(const char *reason) -> void {
  std::fprintf(stderr, "[STUB] MsgViewer::outOfMemory(\"%s\") — aborting\n",
               reason ? reason : "");
  std::abort();
}

auto MsgViewer::show(MsgType, bool, bool, const char *title, const char *fmtStr, ...)
    -> ConfirmDataPtr {
  (void)title;
  if (fmtStr) {
    va_list ap;
    va_start(ap, fmtStr);
    std::fputs("[STUB] MsgViewer::show: ", stderr);
    std::vfprintf(stderr, fmtStr, ap);
    std::fputc('\n', stderr);
    va_end(ap);
  }
  return ConfirmDataPtr{nullptr};
}

// ============================================================================
// TOC::loadFromEpub — only called when pageLocsInstance=true; never triggered
// by the epub test suite where pageLocsInstance remains false.
// ============================================================================

#include "models/toc.hpp"

auto TOC::loadFromEpub(EPub &) -> bool { return true; }

// ============================================================================
// JPegPicture / PngPicture — only instantiated via getPicture() which is
// never called in the epub structural tests.  Stubs satisfy the linker.
// ============================================================================

#include "jpeg_picture.hpp"
#include "png_picture.hpp"

JPegPicture::JPegPicture(std::string, Dim, bool, bool) {}
PngPicture::PngPicture (std::string, Dim, bool)       {}
