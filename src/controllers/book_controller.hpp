// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "viewers/book_viewer.hpp"

class BookController {
public:
  BookController()  = default;
  ~BookController() = default;

  inline auto becomeOwnerOfBook(EPubPtr epubPtr) -> void { epub = std::move(epubPtr); }

  auto inputEvent(const EventMgr::Event &event) -> void;
  auto enter() -> void;
  auto leave(bool goingToDeepSleep = false) -> void;
  bool openBook(const HimemString &bookTitle, const HimemString &bookFilename,
                const PageId &pageId);
  // void put_str(const char *str, int xpos, int ypos);

  inline const PageId &getCurrentPageId() { return currentPageId; }
  inline auto setCurrentPageId(const PageId &pageId) -> void { currentPageId = pageId; }

private:
  static constexpr char const *TAG = "BookController";

  EPubPtr epub{nullptr};

  PageId currentPageId{0, 0};

  BookViewerPtr bookViewer{nullptr};
};

#if __BOOK_CONTROLLER__
  BookController bookController;
#else
  extern BookController bookController;
#endif
