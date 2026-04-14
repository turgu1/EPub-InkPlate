// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/books_dir.hpp"
#include "viewers/books_dir_viewer.hpp"
#include "viewers/page.hpp"

using MatrixBooksDirViewerPtr = himemUniquePtr<class MatrixBooksDirViewer>;
class MatrixBooksDirViewer : public BooksDirViewer {
private:
  static constexpr char const *TAG = "MatrixBooksDirView";

  static const int16_t TITLE_FONT                = 1;
  static const int16_t AUTHOR_FONT               = 2;
  static const int16_t TITLE_FONT_SIZE           = 11;
  static const int16_t AUTHOR_FONT_SIZE          = 9;
  static const int16_t MIN_SPACE_BETWEEN_ENTRIES = 6;
  static const int16_t SPACE_BELOW_INFO          = 10;
  static const int16_t SPACE_ABOVE_PAGENBR       = 5;
  static const int16_t MAX_TITLE_SIZE            = 85;

  int16_t currentItemIdx{-1}; // Relative to the beginning of the page
  int16_t currentBookIdx{-1}; // Relative to the beginning of the complete boolk list
  int16_t currentPageNbr{-1};
  int16_t booksPerPage;
  int16_t columnCount;
  int16_t lineCount;
  int16_t pageCount;
  int16_t firstEntryYpos;
  uint16_t titleFontHeight;
  uint16_t authorFontHeight;
  uint16_t pagenbrFontHeight;
  uint8_t horizSpaceBetweenEntries;
  uint8_t vertSpaceBetweenEntries;

  void showPage(int16_t page_nbr, int16_t hightlight_item_idx);
  void highlight(int16_t item_idx);

  PagePtr page{Page::Make()};

  MatrixBooksDirViewer() = default;

public:
  ~MatrixBooksDirViewer() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> himemUniquePtr<T>;

  static inline auto Make() { return makeUniqueHimem<MatrixBooksDirViewer>(); }

  void setup();

  auto showPageAndHighlight(int16_t book_idx) -> int16_t;
  void highlightBook(int16_t book_idx);
  void clearHighlight();

  auto nextPage() -> int16_t;
  auto prevPage() -> int16_t;
  auto nextItem() -> int16_t;
  auto prevItem() -> int16_t;
  auto nextColumn() -> int16_t;
  auto prevColumn() -> int16_t;

  auto getIndexAt(uint16_t x, uint16_t y) -> int16_t {
    if ((x < 5) || (y < firstEntryYpos)) return -1;
    int16_t line_idx =
        (y - firstEntryYpos) / (BooksDir::coverDim.height + vertSpaceBetweenEntries);
    int16_t column_idx = (x - 5) / (BooksDir::coverDim.width + horizSpaceBetweenEntries);
    if ((line_idx >= lineCount) || (column_idx >= columnCount)) return -1;
    return (currentPageNbr * booksPerPage) + (column_idx * lineCount) + line_idx;
  }
};
