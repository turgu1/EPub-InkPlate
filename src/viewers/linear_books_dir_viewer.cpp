// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __LINEAR_BOOKS_DIR_VIEWER__ 1
#include "viewers/linear_books_dir_viewer.hpp"

#include "picture.hpp"

#include "config.hpp"
#include "fonts.hpp"
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

  topYPos = (Screen::getHeight() - 20 -
             ((booksPerPage * BooksDir::coverDim.height) +
              ((booksPerPage - 1) * SPACE_BETWEEN_ENTRIES))) / 2;

  LOG_D("Books count: {}", booksDir.getBookCount());
}

auto LinearBooksDirViewer::showPage(int16_t pageNbr, int16_t hightlightItemIdx) -> void {
  currentPageNbr = pageNbr;
  currentItemIdx = hightlightItemIdx;

  int16_t bookIdx = pageNbr * booksPerPage;
  int16_t last     = bookIdx + booksPerPage;

  page->setComputeMode(Page::ComputeMode::DISPLAY);

  if (last > booksDir.getBookCount()) { last = booksDir.getBookCount(); }

  uint16_t     xpos = 20 + BooksDir::coverDim.width;
  uint16_t     ypos = topYPos;

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

  for (int itemIdx = 0; bookIdx < last; itemIdx++, bookIdx++) {

    int16_t top_pos = ypos;

    auto    book = booksDir.getBookData(bookIdx);

    if (!book) { break; }
    PicturePtr picture =
      Picture::Make(book->coverDim, (uint8_t *)book->coverBitmap, book->coverSize(), 4);
    page->putPicture(std::move(picture),
                     Pos(10 + BooksDir::coverDim.width - book->coverDim.width, ypos));

    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      if (itemIdx == currentItemIdx) {
        page->putHighlight(
          Dim(Screen::getWidth() - (25 + BooksDir::coverDim.width), BooksDir::coverDim.height),
          Pos(xpos - 5, ypos));
      }
    #endif

    fmt.fontIndex    = TITLE_FONT;
    fmt.fontSize     = TITLE_FONT_SIZE;
    fmt.fontStyle    = FaceStyle::NORMAL;
    fmt.screenTop    = ypos;
    fmt.screenBottom = static_cast<int16_t>(
      Screen::getHeight() - (ypos + BooksDir::coverDim.height + SPACE_BETWEEN_ENTRIES));

    page->setLimits(fmt);
    page->newParagraph(fmt);
    #if EPUB_INKPLATE_BUILD
      if (nvsMgr.idExists(book->id)) { page->addText("[Reading] ", fmt); }
    #endif
    page->addText(book->title, fmt);
    page->endParagraph(fmt);

    fmt.fontIndex = AUTHOR_FONT;
    fmt.fontSize  = AUTHOR_FONT_SIZE;
    fmt.fontStyle = FaceStyle::ITALIC;

    page->newParagraph(fmt);
    page->addText(book->author, fmt);
    page->endParagraph(fmt);

    ypos = top_pos + BooksDir::coverDim.height + SPACE_BETWEEN_ENTRIES;
  }

  ScreenBottom::show(page, pageNbr, pageCount);

  page->paint();
}

auto LinearBooksDirViewer::highlight(int16_t itemIdx) -> void {
  #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
    page->setComputeMode(Page::ComputeMode::DISPLAY);

    if (currentItemIdx != itemIdx) {

      // Clear the highlighting of the current item

      int16_t  bookIdx = currentPageNbr * booksPerPage + currentItemIdx;

      uint16_t xpos = 20 + BooksDir::coverDim.width;
      uint16_t ypos =
        topYPos + (currentItemIdx * (BooksDir::coverDim.height + SPACE_BETWEEN_ENTRIES));

      auto book = booksDir.getBookData(bookIdx);

      if (!book) { return; }

      // TTF * font = appFonts.get(1, 9);

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
        if (nvsMgr.idExists(book->id)) { page->addText("[Reading] ", fmt); }
      #endif
      page->addText(book->title, fmt);
      page->endParagraph(fmt);

      fmt.fontIndex = AUTHOR_FONT;
      fmt.fontSize  = AUTHOR_FONT_SIZE;
      fmt.fontStyle = FaceStyle::ITALIC;

      page->newParagraph(fmt);
      page->addText(book->author, fmt);
      page->endParagraph(fmt);

      // Highlight the new current item

      currentItemIdx = itemIdx;

      bookIdx = currentPageNbr * booksPerPage + currentItemIdx;
      ypos     = topYPos + (currentItemIdx * (BooksDir::coverDim.height + 6));

      book = booksDir.getBookData(bookIdx);

      if (!book) { return; }

      page->putHighlight(
        Dim(Screen::getWidth() - (25 + BooksDir::coverDim.width), BooksDir::coverDim.height),
        Pos(xpos - 5, ypos));

      fmt.fontIndex = TITLE_FONT;
      fmt.fontSize  = TITLE_FONT_SIZE;
      fmt.fontStyle = FaceStyle::NORMAL;
      fmt.screenTop = ypos;
      fmt.screenBottom =
        static_cast<int16_t>(Screen::getHeight() - (ypos + BooksDir::coverDim.width + 20));

      page->setLimits(fmt);
      page->newParagraph(fmt);
      #if EPUB_INKPLATE_BUILD
        if (nvsMgr.idExists(book->id)) { page->addText("[Reading] ", fmt); }
      #endif
      page->addText(book->title, fmt);
      page->endParagraph(fmt);

      fmt.fontIndex = AUTHOR_FONT, fmt.fontSize = AUTHOR_FONT_SIZE,
      fmt.fontStyle = FaceStyle::ITALIC,

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

auto LinearBooksDirViewer::showPageAndHighlight(int16_t bookIdx) -> int16_t {
  int16_t pageNbr = bookIdx / booksPerPage;
  int16_t itemIdx = bookIdx % booksPerPage;

  if (currentPageNbr != pageNbr) {
    showPage(pageNbr, itemIdx);
  } else {
    if (itemIdx != currentItemIdx) { highlight(itemIdx); }
  }
  currentBookIdx = bookIdx;
  return currentBookIdx;
}

auto LinearBooksDirViewer::highlightBook(int16_t bookIdx) -> void {
  highlight(bookIdx % booksPerPage);
  currentBookIdx = bookIdx;
}

auto LinearBooksDirViewer::nextPage() -> int16_t { return nextColumn(); }

auto LinearBooksDirViewer::prevPage() -> int16_t { return prevColumn(); }

auto LinearBooksDirViewer::nextItem() -> int16_t {
  int16_t bookIdx = currentBookIdx + 1;
  if (bookIdx >= booksDir.getBookCount()) {
    bookIdx = booksDir.getBookCount() - 1;
  }
  return showPageAndHighlight(bookIdx);
}

auto LinearBooksDirViewer::prevItem() -> int16_t {
  int16_t bookIdx = currentBookIdx - 1;
  if (bookIdx < 0) { bookIdx = 0; }
  return showPageAndHighlight(bookIdx);
}

auto LinearBooksDirViewer::nextColumn() -> int16_t {
  int16_t bookIdx = currentBookIdx + booksPerPage;
  if (bookIdx >= booksDir.getBookCount()) {
    bookIdx = booksDir.getBookCount() - 1;
  } else {
    bookIdx = (bookIdx / booksPerPage) * booksPerPage;
  }
  return showPageAndHighlight(bookIdx);
}

auto LinearBooksDirViewer::prevColumn() -> int16_t {
  int16_t bookIdx = currentBookIdx - booksPerPage;
  if (bookIdx < 0) {
    bookIdx = 0;
  }
  else {
    bookIdx = (bookIdx / booksPerPage) * booksPerPage;
  }
  return showPageAndHighlight(bookIdx);
}
