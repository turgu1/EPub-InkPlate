// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "viewers/books_dir_viewer.hpp"
#include "viewers/linear_books_dir_viewer.hpp"
#include "viewers/matrix_books_dir_viewer.hpp"

class BooksDirController {
private:
  static constexpr char const *TAG = "BooksDirController";

  const uint8_t LINEAR_VIEWER = 0;
  const uint8_t MATRIX_VIEWER = 1;

  int32_t bookOffset{0};
  int16_t currentBookIndex{0};
  int16_t lastReadBookIndex{0};
  std::string bookFilename;
  bool bookWasShown{false};

  PageId bookPageId{0, 0};
  BooksDirViewerPtr booksDirViewer{nullptr};

  int8_t viewerId{0};

public:
  BooksDirController()  = default;
  ~BooksDirController() = default;

  auto setup() -> void;
  auto inputEvent(const EventMgr::Event &event) -> void;
  auto enter() -> void;
  auto leave(bool goingToDeepSleep = false) -> void;
  auto openBookFromPath(const char *bookPath, const PageId &pageId = {0, 0}) -> bool;
  auto saveLastBook(const PageId &pageId, bool goingToDeepSleep) -> void;
  auto showLastBook() -> void;
  auto newOrientation() -> void {
    if (booksDirViewer != nullptr) booksDirViewer->setup();
  }

  [[nodiscard]] inline auto getCurrentBookIndex() -> int16_t { return currentBookIndex; }
  inline auto setCurrentBookIndex(int16_t idx) -> void { currentBookIndex = idx; }
};

#if __BOOKS_DIR_CONTROLLER__
  BooksDirController booksDirController;
#else
  extern BooksDirController booksDirController;
#endif
