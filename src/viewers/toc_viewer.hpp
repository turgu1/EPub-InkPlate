// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/epub.hpp"
#include "viewers/page.hpp"

using TocViewerPtr = HimemUniquePtr<class TocViewer>;
class TocViewer {
private:
  static constexpr char const *TAG = "TocView";

  static const int16_t titleFont     = 1;
  static const int16_t entryFont     = 1;
  static const int16_t entryFontSize = 11;
  static const int16_t titleFontSize = 14;
  static const int16_t maxTitleSize  = 90;
  static const int16_t titleYPos     = 20;

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    static const int16_t entryHeight    = 40;
    static const int16_t firstEntryYPos = 100;
  #else
    static const int16_t entryHeight    = 26;
    static const int16_t firstEntryYPos = 80;
  #endif

  int16_t currentEntryIdx{-1};
  int16_t currentScreenIdx{-1};
  int16_t currentPageNbr{-1};
  int16_t entriesPerPage{-1};
  int16_t pageCount{-1};

  auto showPage(int16_t pageNbr, int16_t highlightedScreenIdx) -> void;
  auto highlight(int16_t screenIdx) -> void;

  PagePtr page{Page::Make(appFonts)};
  EPubPtr &epub;

  TocViewer(EPubPtr &theEpub) : epub(theEpub) {}

public:
  ~TocViewer() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> HimemUniquePtr<T>;

  static inline auto Make(EPubPtr &theEpub) { return makeUniqueHimem<TocViewer>(theEpub); }

  auto setup() -> void;

  auto showPageAndHighlight(int16_t entryIdx) -> int16_t;
  auto highlightEntry(int16_t entryIdx) -> void;
  auto clearHighlight() -> void {}

  auto nextPage() -> int16_t;
  auto prevPage() -> int16_t;
  auto nextItem() -> int16_t;
  auto prevItem() -> int16_t;
  auto nextColumn() -> int16_t;
  auto prevColumn() -> int16_t;

  auto getIndexAt(uint16_t x, uint16_t y) -> int16_t {
    int16_t idx = (y - firstEntryYPos) / entryHeight;
    return (idx >= entriesPerPage) ? -1 : (currentPageNbr * entriesPerPage) + idx;
  }
};
