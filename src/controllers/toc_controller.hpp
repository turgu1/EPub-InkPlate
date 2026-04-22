// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "viewers/toc_viewer.hpp"

class TocController {
private:
  static constexpr char const *TAG = "TocController";

  int16_t currentEntryIndex;
  int16_t currentBookIndex;

  TocViewerPtr tocViewer{nullptr};

  EPubPtr epub{nullptr};

public:
  TocController() : currentEntryIndex(-1), currentBookIndex(-1) {}

  inline auto becomeOwnerOfBook(EPubPtr epubPtr) -> void { epub = std::move(epubPtr); }

  auto inputEvent(const EventMgr::Event &event) -> void;
  auto enter() -> void;
  auto leave(bool goingToDeepSleep = false) -> void;
};

#if __TOC_CONTROLLER__
  TocController tocController;
#else
  extern TocController tocController;
#endif
