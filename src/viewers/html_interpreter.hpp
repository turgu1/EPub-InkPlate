// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/dom.hpp"
#include "models/epub.hpp"
#include "pugixml.hpp"
#include "viewers/page.hpp"

using namespace pugi;

// The HTMLInterpreter class is used to process the content of a book file (called item),
// preparing it for display through the BookViewer class or page location computation through
// the PageLocs class.
//
// Tags that are understood are used to create a partial DOM structure that is then passed through
// the CSS match algorithm to transform the viewing parameters as gathered in a Page::Format struc.

class HTMLInterpreter {
protected:
  static constexpr char const *TAG = "HTMLInterpreter";

  EPubPtr &epub;
  PagePtr &page;
  DOMPtr &dom;
  Page::ComputeMode computeMode;
  const EPub::ItemInfo &itemInfo;

  int32_t currentOffset{0}; ///< Where we are in current item
  int32_t startOffset{0};
  int32_t endOffset{0};

  ///< This is used to adjust the offset when we need to add content that is not part of the
  ///< original text, such as images. It is added to the currentOffset when checking for page
  ///< limits.
  int32_t offsetAdjustment{0};

  bool showPictures{false};
  bool started{false};
  // bool beginning_of_page;

  bool showTheState{false};
  int16_t fromPage{-1}, toPage{-1};
  int16_t maxLevel{0};

  HimemPool<Page::Format> fmtPool;

  // The pageEnd method is responsible of doing post-processing once
  // the end of a page has been detected (the page.isFull() method returns true or
  // the process reached the pageEnd offset). This is specific for each of the book_viewer
  // and the page location computation processes.
  virtual auto pageEnd(const Page::Format &fmt) -> bool = 0;

public:
  HTMLInterpreter(EPubPtr &theEpub, PagePtr &thePage, DOMPtr &theDom, Page::ComputeMode theCompMode,
                  const EPub::ItemInfo &theItem)
      : epub(theEpub), page(thePage), dom(theDom), computeMode(theCompMode), itemInfo(theItem) {}

  virtual ~HTMLInterpreter() {}

  auto setLimits(int32_t start, int32_t end, bool showImgs) -> void {
    started = false;
    // beginning_of_page  = false;
    currentOffset = 0;
    startOffset   = start;
    endOffset     = end;
    showPictures  = showImgs;
    page->setComputeMode(Page::ComputeMode::MOVE);
  }

  auto buildPagesRecurse(xml_node node, Page::Format &fmt, DOM::Node *domNode, int16_t level)
      -> bool;

  auto checkForCompletion() -> void {
    if (currentOffset != endOffset) {
      LOG_E("Current page offset and end of page offset differ: %" PRIi32 " vs %" PRIi32,
            currentOffset, endOffset);
    }
  }

  auto showState(const char *caption, const Page::Format &fmt,
                 DOM::Node *domCurrentNode = nullptr /*, CSSPtr elementCss = nullptr*/) -> void {
    if (showTheState) {
      std::cout << caption << " Offset:" << currentOffset << " ";
      page->showControls("  ");
      std::cout << "     ";
      page->showFmt(fmt, "  ");
    }
  }

  auto setPagesToShowState(int16_t from, int16_t to) -> void {
    fromPage = from;
    toPage   = to;
  }

  auto checkPageToShow(int16_t page) -> void {
    showTheState = (fromPage <= page) && (page <= toPage);
  }

  [[nodiscard]] inline auto checkIfStarted() -> bool {
    if (!started) {
      if ((started = (currentOffset >= startOffset))) {
        page->setComputeMode(computeMode);
        page->clean();
        LOG_D("---- PAGE START ----");
      }
    }
    return started;
  }

  [[nodiscard]] inline auto atEnd() -> bool { return currentOffset >= endOffset; }

  [[nodiscard]] inline auto duplicateFmt(const Page::Format &fmt) -> Page::Format * {
    Page::Format *newFmt = fmtPool.allocate();
    *newFmt              = fmt;
    return newFmt;
  }

  inline auto releaseFmt(Page::Format *fmt) -> void { fmtPool.deallocate(fmt); }

  inline auto showStat() -> void { LOG_D("Max Level: %d", maxLevel); }
};