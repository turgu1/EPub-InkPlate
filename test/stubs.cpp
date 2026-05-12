// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test/stubs.cpp — minimal stubs for symbols required by the test build
//                  that live in hardware/viewer code excluded from the tests.
//
// Stubs provided here:
//   • Fonts global              — appFonts definition for the test link
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
// Fonts declarations (global appFonts is defined by components/fonts/src/fonts.cpp).
// ============================================================================

#include "fonts.hpp" // includes char_pool.hpp, font.hpp — no GTK

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

#include "viewers/msg_viewer.hpp" // → test/stubs/viewers/msg_viewer.hpp

auto MsgViewer::outOfMemory(const char *reason) -> void {
  std::fprintf(stderr, "[STUB] MsgViewer::outOfMemory(\"%s\") — aborting\n", reason ? reason : "");
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

JPegPicture::JPegPicture(const HimemString &, Dim, bool, bool) {}
PngPicture::PngPicture(const HimemString &, Dim, bool) {}

// ============================================================================
// PageLocs — epub.cpp::closeFile() references the global `pageLocs` object
// and calls stopControlTask().  The test build excludes page_locs.cpp, so we
// provide the global object definition and an empty stub implementation here.
// ============================================================================

#include "models/page_locs.hpp" // brings in full PageLocs class definition

PageLocs pageLocs; // the global instance referenced by epub.cpp

auto PageLocs::stopControlTask() -> void {}
