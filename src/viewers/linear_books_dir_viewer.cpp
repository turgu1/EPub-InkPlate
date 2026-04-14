// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __LINEAR_BOOKS_DIR_VIEWER__ 1
#include "viewers/linear_books_dir_viewer.hpp"

#include "picture.hpp"

#include "models/config.hpp"
#include "models/fonts.hpp"
#include "viewers/page.hpp"
#include "viewers/screen_bottom.hpp"

#if EPUB_INKPLATE_BUILD
  #include "models/nvs_mgr.hpp"
  #include "viewers/battery_viewer.hpp"
#endif

#include "screen.hpp"

#include <iomanip>

auto LinearBooksDirViewer::setup() -> void {
  booksPerPage = (Screen::getHeight() - FIRST_ENTRY_YPOS - 20 + SPACE_BETWEEN_ENTRIES) /
                 (BooksDir::coverDim.height + SPACE_BETWEEN_ENTRIES);
  pageCount    = (booksDir.getBookCount() + booksPerPage - 1) / booksPerPage;

  currentPageNbr = -1;
  currentBookIdx = -1;
  currentItemIdx = -1;

  LOG_D("Books count: %d", booksDir.getBookCount());
}

auto LinearBooksDirViewer::showPage(int16_t page_nbr, int16_t hightlight_item_idx) -> void {
  currentPageNbr = page_nbr;
  currentItemIdx = hightlight_item_idx;

  int16_t book_idx = page_nbr * booksPerPage;
  int16_t last     = book_idx + booksPerPage;

  page->setComputeMode(Page::ComputeMode::DISPLAY);

  if (last > booksDir.getBookCount()) last = booksDir.getBookCount();

  uint16_t xpos = 20 + BooksDir::coverDim.width;
  uint16_t ypos = FIRST_ENTRY_YPOS;

  Page::Format fmt = {
      .lineHeightFactor = 0.9,
      .fontIndex        = TITLE_FONT,
      .fontSize         = TITLE_FONT_SIZE,
      .screenLeft       = xpos,
      .screenRight      = 10,
      .screenTop        = ypos,
      .screenBottom =
          static_cast<uint16_t>(Screen::getHeight() - (ypos + BooksDir::coverDim.width + 20)),
  };

  page->start(fmt);

  for (int item_idx = 0; book_idx < last; item_idx++, book_idx++) {

    int16_t top_pos = ypos;

    auto book = booksDir.getBookData(book_idx);

    if (!book) break;
    PicturePtr picture =
        Picture::Make(book->coverDim, (uint8_t *)book->coverBitmap, book->coverSize());
    page->putPicture(std::move(picture),
                     Pos(10 + BooksDir::coverDim.width - book->coverDim.width, ypos));

    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      if (item_idx == currentItemIdx) {
        page->putHighlight(
            Dim(Screen::getWidth() - (25 + BooksDir::coverDim.width), BooksDir::coverDim.height),
            Pos(xpos - 5, ypos));
      }
    #endif

    fmt.fontIndex    = TITLE_FONT;
    fmt.fontSize     = TITLE_FONT_SIZE;
    fmt.fontStyle    = Fonts::FaceStyle::NORMAL;
    fmt.screenTop    = ypos;
    fmt.screenBottom = static_cast<int16_t>(
        Screen::getHeight() - (ypos + BooksDir::coverDim.height + SPACE_BETWEEN_ENTRIES));

    page->setLimits(fmt);
    page->newParagraph(fmt);
    #if EPUB_INKPLATE_BUILD
      if (nvsMgr.idExists(book->id)) page->addText("[Reading] ", fmt);
    #endif
    page->addText(book->title, fmt);
    page->endParagraph(fmt);

    fmt.fontIndex = AUTHOR_FONT;
    fmt.fontSize  = AUTHOR_FONT_SIZE;
    fmt.fontStyle = Fonts::FaceStyle::ITALIC;

    page->newParagraph(fmt);
    page->addText(book->author, fmt);
    page->endParagraph(fmt);

    ypos = top_pos + BooksDir::coverDim.height + SPACE_BETWEEN_ENTRIES;
  }

  ScreenBottom::show(page, page_nbr, pageCount);

  page->paint();
}

auto LinearBooksDirViewer::highlight(int16_t item_idx) -> void {
  #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
    page->setComputeMode(Page::ComputeMode::DISPLAY);

    if (currentItemIdx != item_idx) {

      // Clear the highlighting of the current item

      int16_t book_idx = currentPageNbr * booksPerPage + currentItemIdx;

      uint16_t xpos = 20 + BooksDir::coverDim.width;
      uint16_t ypos =
          FIRST_ENTRY_YPOS + (currentItemIdx * (BooksDir::coverDim.height + SPACE_BETWEEN_ENTRIES));

      auto book = booksDir.getBookData(book_idx);

      if (!book) return;

      // TTF * font = fonts.get(1, 9);

      Page::Format fmt = {
          .lineHeightFactor = 0.9,
          .fontIndex        = TITLE_FONT,
          .fontSize         = TITLE_FONT_SIZE,
          .screenLeft       = xpos,
          .screenRight      = 10,
          .screenTop        = ypos,
          .screenBottom =
              static_cast<uint16_t>(Screen::getHeight() - (ypos + BooksDir::coverDim.width + 20)),
      };

      page->start(fmt);

      page->clearHighlight(
          Dim(Screen::getWidth() - (25 + BooksDir::coverDim.width), BooksDir::coverDim.height),
          Pos(xpos - 5, ypos));

      page->setLimits(fmt);
      page->newParagraph(fmt);
      #if EPUB_INKPLATE_BUILD
        if (nvsMgr.idExists(book->id)) page->addText("[Reading] ", fmt);
      #endif
      page->addText(book->title, fmt);
      page->endParagraph(fmt);

      fmt.fontIndex = AUTHOR_FONT;
      fmt.fontSize  = AUTHOR_FONT_SIZE;
      fmt.fontStyle = Fonts::FaceStyle::ITALIC;

      page->newParagraph(fmt);
      page->addText(book->author, fmt);
      page->endParagraph(fmt);

      // Highlight the new current item

      currentItemIdx = item_idx;

      book_idx = currentPageNbr * booksPerPage + currentItemIdx;
      ypos     = FIRST_ENTRY_YPOS + (currentItemIdx * (BooksDir::coverDim.height + 6));

      book = booksDir.getBookData(book_idx);

      if (!book) return;

      page->putHighlight(
          Dim(Screen::getWidth() - (25 + BooksDir::coverDim.width), BooksDir::coverDim.height),
          Pos(xpos - 5, ypos));

      fmt.fontIndex = TITLE_FONT;
      fmt.fontSize  = TITLE_FONT_SIZE;
      fmt.fontStyle = Fonts::FaceStyle::NORMAL;
      fmt.screenTop = ypos;
      fmt.screenBottom =
          static_cast<int16_t>(Screen::getHeight() - (ypos + BooksDir::coverDim.width + 20));

      page->setLimits(fmt);
      page->newParagraph(fmt);
      #if EPUB_INKPLATE_BUILD
        if (nvsMgr.idExists(book->id)) page->addText("[Reading] ", fmt);
      #endif
      page->addText(book->title, fmt);
      page->endParagraph(fmt);

      fmt.fontIndex = AUTHOR_FONT, fmt.fontSize = AUTHOR_FONT_SIZE,
      fmt.fontStyle = Fonts::FaceStyle::ITALIC,

      page->newParagraph(fmt);
      page->addText(book->author, fmt);
      page->endParagraph(fmt);

      #if EPUB_INKPLATE_BUILD
        BatteryViewer::show(page);
      #endif

      page->paint(false);
    }
  #endif
}

auto LinearBooksDirViewer::showPageAndHighlight(int16_t book_idx) -> int16_t {
  int16_t page_nbr = book_idx / booksPerPage;
  int16_t item_idx = book_idx % booksPerPage;

  if (currentPageNbr != page_nbr) {
    showPage(page_nbr, item_idx);
  } else {
    if (item_idx != currentItemIdx) highlight(item_idx);
  }
  currentBookIdx = book_idx;
  return currentBookIdx;
}

auto LinearBooksDirViewer::highlightBook(int16_t book_idx) -> void {
  highlight(book_idx % booksPerPage);
  currentBookIdx = book_idx;
}

auto LinearBooksDirViewer::nextPage() -> int16_t { return nextColumn(); }

auto LinearBooksDirViewer::prevPage() -> int16_t { return prevColumn(); }

auto LinearBooksDirViewer::nextItem() -> int16_t {
  int16_t book_idx = currentBookIdx + 1;
  if (book_idx >= booksDir.getBookCount()) {
    book_idx = booksDir.getBookCount() - 1;
  }
  return showPageAndHighlight(book_idx);
}

auto LinearBooksDirViewer::prevItem() -> int16_t {
  int16_t book_idx = currentBookIdx - 1;
  if (book_idx < 0) book_idx = 0;
  return showPageAndHighlight(book_idx);
}

auto LinearBooksDirViewer::nextColumn() -> int16_t {
  int16_t book_idx = currentBookIdx + booksPerPage;
  if (book_idx >= booksDir.getBookCount()) {
    book_idx = booksDir.getBookCount() - 1;
  } else {
    book_idx = (book_idx / booksPerPage) * booksPerPage;
  }
  return showPageAndHighlight(book_idx);
}

auto LinearBooksDirViewer::prevColumn() -> int16_t {
  int16_t book_idx = currentBookIdx - booksPerPage;
  if (book_idx < 0)
    book_idx = 0;
  else
    book_idx = (book_idx / booksPerPage) * booksPerPage;
  return showPageAndHighlight(book_idx);
}
