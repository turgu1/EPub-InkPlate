// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test/stubs/viewers/msg_viewer.hpp — minimal stub that replaces the real
// src/viewers/msg_viewer.hpp in the headless test build.
//
// The real header includes screen.hpp (→ gtk/gtk.h) and event_mgr.hpp, both
// of which are unavailable in the test environment.  This stub forward-
// declares just enough for epub.cpp to compile: the MsgType enum, the
// ConfirmDataPtr alias, show(), and outOfMemory().
// ---------------------------------------------------------------------------

#pragma once
#include "global.hpp"
#include "himem.hpp"
#include "screen.hpp"   // stub version (test/stubs/screen.hpp) — provides Screen::IDENT etc.

// Minimal stub ConfirmData so that HimemUniquePtr<ConfirmData> is not incomplete.
class ConfirmData {
public:
  bool ok{false};
  ConfirmData() = default;
};

class MsgViewer {
public:
  enum class MsgType : uint8_t { INFO, ALERT, WIFI, BOOK, FONT };

  using ConfirmDataPtr = HimemUniquePtr<ConfirmData>;

  /// Show a user-visible message.  Returns nullptr in the test stub.
  static auto show(MsgType msgType, bool pressAKey, bool clearScreen,
                   const char *title, const char *fmtStr, ...) -> ConfirmDataPtr;

  /// Called when a critical allocation fails.  Aborts in the test stub.
  static auto outOfMemory(const char *reason) -> void;
};
