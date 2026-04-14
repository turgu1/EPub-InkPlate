// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/books_dir.hpp"
#include "viewers/books_dir_viewer.hpp"
#include "viewers/page.hpp"

using LinearBooksDirViewerPtr = himemUniquePtr<class LinearBooksDirViewer>;
class LinearBooksDirViewer : public BooksDirViewer {
private:
  static constexpr char const *TAG = "LinearBooksDirView";

  static const int16_t TITLE_FONT            = 1;
  static const int16_t AUTHOR_FONT           = 2;
  static const int16_t TITLE_FONT_SIZE       = 11;
  static const int16_t AUTHOR_FONT_SIZE      = 9;
  static const int16_t FIRST_ENTRY_YPOS      = 5;
  static const int16_t SPACE_BETWEEN_ENTRIES = 6;
  static const int16_t MAX_TITLE_SIZE        = 85;

  int16_t currentItemIdx{-1};
  int16_t currentBookIdx{-1};
  int16_t currentPageNbr{-1};
  int16_t booksPerPage{-1};
  int16_t pageCount{-1};

  void showPage(int16_t pageNbr, int16_t hightlight_item_idx);
  void highlight(int16_t item_idx);

  PagePtr page{Page::Make()};

  LinearBooksDirViewer() = default;

public:
  ~LinearBooksDirViewer() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> himemUniquePtr<T>;

  static inline auto Make() { return makeUniqueHimem<LinearBooksDirViewer>(); }

  void setup();

  auto showPageAndHighlight(int16_t book_idx) -> int16_t;
  void highlightBook(int16_t book_idx);
  void clearHighlight() {}

  auto nextPage() -> int16_t;
  auto prevPage() -> int16_t;
  auto nextItem() -> int16_t;
  auto prevItem() -> int16_t;
  auto nextColumn() -> int16_t;
  auto prevColumn() -> int16_t;

  auto getIndexAt(uint16_t x, uint16_t y) -> int16_t {
    int16_t idx = (y - FIRST_ENTRY_YPOS) / (BooksDir::coverDim.height + SPACE_BETWEEN_ENTRIES);
    return (idx >= booksPerPage) ? -1 : (currentPageNbr * booksPerPage) + idx;
  }
};
