// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "viewers/toc_viewer.hpp"

#include "models/config.hpp"
#include "models/fonts.hpp"
#include "viewers/page.hpp"
#include "viewers/screen_bottom.hpp"

#if EPUB_INKPLATE_BUILD
  #include "viewers/battery_viewer.hpp"
#endif

#include "screen.hpp"

#include <iomanip>

auto TocViewer::setup() -> void {
  entriesPerPage = (Screen::getHeight() - firstEntryYPos - 20) / entryHeight;
  pageCount      = (epub->toc->getEntryCount() + entriesPerPage - 1) / entriesPerPage;

  currentPageNbr   = -1;
  currentScreenIdx = -1;
  currentEntryIdx  = -1;

  LOG_D("TOC entry count: %d", epub->toc->getEntryCount());
}

auto TocViewer::showPage(int16_t pageNbr, int16_t highlightedScreenIdx) -> void {
  currentPageNbr   = pageNbr;
  currentScreenIdx = highlightedScreenIdx;

  int16_t entryIdx = pageNbr * entriesPerPage;  // entry idx in the current page
  int16_t lastIdx  = entryIdx + entriesPerPage; // last entry idx in the current page

  if (lastIdx > epub->toc->getEntryCount()) lastIdx = epub->toc->getEntryCount();

  uint16_t xpos = 20;
  uint16_t ypos = titleYPos;

  page->setComputeMode(Page::ComputeMode::DISPLAY);

  Page::Format fmt = {
      .lineHeightFactor = 0.8,
      .fontIndex        = titleFont,
      .fontSize         = titleFontSize,
      .screenLeft       = xpos,
      .screenRight      = 10,
      .screenTop        = ypos,
      .screenBottom     = static_cast<uint16_t>(Screen::getHeight() - (ypos + maxTitleSize + 20)),
      .fontStyle        = FaceStyle::BOLD,
      .align            = CSS::Align::CENTER,
  };

  page->start(fmt);

  page->setLimits(fmt);
  page->newParagraph(fmt);
  page->addText(epub->getTitle(), fmt);
  page->endParagraph(fmt);

  ypos = firstEntryYPos;

  fmt.fontIndex = entryFont;
  fmt.fontSize  = entryFontSize;
  fmt.fontStyle = FaceStyle::NORMAL;
  fmt.align     = CSS::Align::LEFT;

  for (int16_t screenIdx = 0; entryIdx < lastIdx; ++screenIdx, ++entryIdx) {

    const TOC::EntryRecord &entry = epub->toc->getEntry(entryIdx);

    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      if (screenIdx == currentScreenIdx) {
        page->putHighlight(Dim(Screen::getWidth() - 30, entryHeight + 5), Pos(15, ypos));
      }
    #endif

    fmt.screenLeft   = 20 + (entry.level * 20);
    fmt.screenTop    = ypos;
    fmt.screenBottom = static_cast<int16_t>(Screen::getHeight() - (ypos + entryHeight));

    page->setLimits(fmt);
    page->newParagraph(fmt);
    page->addText(entry.label, fmt);
    page->endParagraph(fmt);

    ypos += entryHeight;
  }

  ScreenBottom::show(page, currentPageNbr, pageCount);

  page->paint();
}

auto TocViewer::highlight(int16_t screenIdx) -> void {
  #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
    page->setComputeMode(Page::ComputeMode::DISPLAY);

    if (currentScreenIdx != screenIdx) {

      // Clear the highlighting of the current item

      int16_t entryIdx = currentPageNbr * entriesPerPage + currentScreenIdx;

      const TOC::EntryRecord &entry = epub->toc->getEntry(entryIdx);

      uint16_t xpos = 20 + (entry.level * 20);
      uint16_t ypos = firstEntryYPos + (currentScreenIdx * entryHeight);

      Page::Format fmt = {
          .lineHeightFactor = 0.8,
          .fontIndex        = entryFont,
          .fontSize         = entryFontSize,
          .screenLeft       = xpos,
          .screenRight      = 10,
          .screenTop        = ypos,
          .screenBottom = static_cast<uint16_t>(Screen::getHeight() - (ypos + entryHeight + 20)),
      };

      page->start(fmt);

      page->clearHighlight(Dim(Screen::getWidth() - 30, entryHeight + 5), Pos(15, ypos));

      page->setLimits(fmt);
      page->newParagraph(fmt);
      page->addText(entry.label, fmt);
      page->endParagraph(fmt);

      // Highlight the new current entry

      currentScreenIdx = screenIdx;

      entryIdx = currentPageNbr * entriesPerPage + currentScreenIdx;
      ypos     = firstEntryYPos + (currentScreenIdx * entryHeight);

      const TOC::EntryRecord &selectedEntry = epub->toc->getEntry(entryIdx);

      page->putHighlight(Dim(Screen::getWidth() - 30, entryHeight + 5), Pos(15, ypos));

      fmt.screenLeft = 20 + (selectedEntry.level * 20);
      fmt.screenTop  = ypos;

      page->setLimits(fmt);
      page->newParagraph(fmt);
      page->addText(selectedEntry.label, fmt);
      page->endParagraph(fmt);

      ScreenBottom::show(page, currentPageNbr, pageCount);

      page->paint(false);
    }
  #endif
}

auto TocViewer::showPageAndHighlight(int16_t entryIdx) -> int16_t {
  int16_t pageNbr   = entryIdx / entriesPerPage;
  int16_t screenIdx = entryIdx % entriesPerPage;

  if (currentPageNbr != pageNbr) {
    showPage(pageNbr, screenIdx);
  } else {
    if (screenIdx != currentScreenIdx) highlight(screenIdx);
  }

  currentEntryIdx = entryIdx;
  return currentEntryIdx;
}

auto TocViewer::highlightEntry(int16_t entryIdx) -> void {
  highlight(entryIdx % entriesPerPage);
  currentEntryIdx = entryIdx;
}

auto TocViewer::nextPage() -> int16_t { return nextColumn(); }

auto TocViewer::prevPage() -> int16_t { return prevColumn(); }

auto TocViewer::nextItem() -> int16_t {
  int16_t entryIdx = currentEntryIdx + 1;
  if (entryIdx >= epub->toc->getEntryCount()) {
    entryIdx = epub->toc->getEntryCount() - 1;
  }
  return showPageAndHighlight(entryIdx);
}

auto TocViewer::prevItem() -> int16_t {
  int16_t entryIdx = currentEntryIdx - 1;
  if (entryIdx < 0) entryIdx = 0;
  return showPageAndHighlight(entryIdx);
}

auto TocViewer::nextColumn() -> int16_t {
  int16_t entryIdx = currentEntryIdx + entriesPerPage;
  if (entryIdx >= epub->toc->getEntryCount()) {
    entryIdx = epub->toc->getEntryCount() - 1;
  } else {
    entryIdx = (entryIdx / entriesPerPage) * entriesPerPage;
  }
  return showPageAndHighlight(entryIdx);
}

auto TocViewer::prevColumn() -> int16_t {
  int16_t entryIdx = currentEntryIdx - entriesPerPage;
  if (entryIdx < 0)
    entryIdx = 0;
  else
    entryIdx = (entryIdx / entriesPerPage) * entriesPerPage;
  return showPageAndHighlight(entryIdx);
}
