// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test/valgrind_stubs.cpp — minimal stubs for the Linux S5 Valgrind build.
//
// This file provides:
//   • Screen singleton  (headless, no GTK — via __SCREEN__ guard in stub header)
//   • MsgViewer stubs   (abort on OOM, log-only show/confirm)
//   • JPegPicture / PngPicture constructors  (never called in pageLocs mode)
//   • TOC::loadFromEpub stub                 (real TOC is populated by retriever)
//   • ScreenBottom::dw[]                     (static data required by linker)
//
// Fonts, Config, PageLocs, EPub etc. are compiled from their real sources.
// ---------------------------------------------------------------------------

// --- Screen singleton (headless) -------------------------------------------
// #define __SCREEN__ 1 here so that test/stubs/screen.hpp emits the singleton.
#define __SCREEN__ 1
#include "screen.hpp" // → test/stubs/screen.hpp (via include-path ordering)

// --- MsgViewer stubs --------------------------------------------------------
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// test/stubs/viewers/msg_viewer.hpp is picked up via include path
#include "viewers/msg_viewer.hpp"

auto MsgViewer::outOfMemory(const char *reason) -> void {
  std::fprintf(stderr, "[VALGRIND] MsgViewer::outOfMemory(\"%s\") — aborting\n",
               reason ? reason : "");
  std::abort();
}

auto MsgViewer::show(MsgType, bool, bool, const char *title, const char *fmtStr, ...)
    -> ConfirmDataPtr {
  if (fmtStr) {
    va_list ap;
    va_start(ap, fmtStr);
    std::fputs("[VALGRIND] MsgViewer::show: ", stderr);
    if (title) std::fprintf(stderr, "[%s] ", title);
    std::vfprintf(stderr, fmtStr, ap);
    std::fputc('\n', stderr);
    va_end(ap);
  }
  return ConfirmDataPtr{nullptr};
}

// --- Picture constructors are provided by the real jpeg_picture.cpp and
// png_picture.cpp which are included in the Valgrind build sources.
// No stubs needed here.

// --- TOC::loadFromEpub stub -------------------------------------------------
// The real retriever creates a TOC and calls save() at the end; loadFromEpub()
// is called during epub->open() when pageLocsInstance==true.  The retriever
// normally provides a full implementation — include the real toc.cpp instead.
// This stub is only needed if toc.cpp is excluded from the Valgrind build.
// (Currently toc.cpp IS included; this stub is therefore commented out.)
//
// #include "models/toc.hpp"
// auto TOC::loadFromEpub(EPub &) -> bool { return true; }

// --- ScreenBottom::dw[] ----------------------------------------------------
// screen_bottom.cpp is excluded (it pulls in battery_viewer / clock).
// ScreenBottom::show() is never called by pageLocs, but the static array
// is a non-inline symbol that the linker may require.
#include "viewers/screen_bottom.hpp"
const std::string ScreenBottom::dw[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// --- eventMgr global stub ---------------------------------------------------
// page_locs.cpp calls eventMgr.setStayOn() which is an inline method on the
// EventMgr instance.  We provide a stub instance here.
#define __EVENT_MGR__ 1
#include "controllers/event_mgr.hpp"

auto EventMgr::someEventWaiting() -> bool { return false; }
