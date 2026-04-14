// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MATRIX_BOOKS_DIR_VIEWER__ 1
#include "viewers/matrix_books_dir_viewer.hpp"

#include "picture.hpp"

#include "models/config.hpp"
#include "models/fonts.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/page.hpp"
#include "viewers/screen_bottom.hpp"

#if EPUB_INKPLATE_BUILD
  #include "models/nvs_mgr.hpp"
  #include "viewers/battery_viewer.hpp"
#endif

#include "screen.hpp"

#include <iomanip>

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  static const std::string TOUCH_AND_HOLD_STR = "Touch and hold cover for info. Tap to open.";
#endif

auto MatrixBooksDirViewer::setup() -> void {
  Font *font      = fonts.get(TITLE_FONT);
  titleFontHeight = font->getLineHeight(TITLE_FONT_SIZE) * 0.8 + 5;

  font             = fonts.get(AUTHOR_FONT);
  authorFontHeight = font->getLineHeight(AUTHOR_FONT_SIZE) * 0.8;

  font              = fonts.get(ScreenBottom::FONT);
  pagenbrFontHeight = font->getLineHeight(ScreenBottom::FONT_SIZE);

  firstEntryYpos = (titleFontHeight << 1) + authorFontHeight + SPACE_BELOW_INFO + 25;

  lineCount = (Screen::getHeight() - firstEntryYpos - pagenbrFontHeight - SPACE_ABOVE_PAGENBR +
               MIN_SPACE_BETWEEN_ENTRIES) /
              (BooksDir::coverDim.height + MIN_SPACE_BETWEEN_ENTRIES);

  columnCount = (Screen::getWidth() - 10 + MIN_SPACE_BETWEEN_ENTRIES) /
                (BooksDir::coverDim.width + MIN_SPACE_BETWEEN_ENTRIES);

  horizSpaceBetweenEntries =
      (Screen::getWidth() - 10 - (BooksDir::coverDim.width * columnCount)) / (columnCount - 1);
  vertSpaceBetweenEntries = (Screen::getHeight() - firstEntryYpos - pagenbrFontHeight -
                             SPACE_ABOVE_PAGENBR - (BooksDir::coverDim.height * lineCount)) /
                            (lineCount - 1);
  booksPerPage            = lineCount * columnCount;
  pageCount               = (booksDir.getBookCount() + booksPerPage - 1) / booksPerPage;

  currentPageNbr = -1;
  currentBookIdx = -1;
  currentItemIdx = -1;
}

auto MatrixBooksDirViewer::showPage(int16_t page_nbr, int16_t hightlight_item_idx) -> void {

  currentPageNbr = page_nbr; // First page == 0
  currentItemIdx = hightlight_item_idx;

  int16_t book_idx = page_nbr * booksPerPage;
  int16_t last     = book_idx + booksPerPage;

  page->setComputeMode(Page::ComputeMode::DISPLAY);

  if (last > booksDir.getBookCount()) last = booksDir.getBookCount();

  int16_t xpos     = 5;
  int16_t ypos     = firstEntryYpos;
  int16_t line_pos = 0;

  Page::Format fmt = {
      .lineHeightFactor = 0.9,
      .fontIndex         = TITLE_FONT,
      .fontSize           = TITLE_FONT_SIZE,
      .screenLeft        = 20,
      .screenRight       = 20,
      .screenTop         = 18,
      .screenBottom      = 100,
      .align              = CSS::Align::CENTER,
  };

  page->start(fmt);

  for (int item_idx = 0; book_idx < last; item_idx++, book_idx++) {

    auto book = booksDir.getBookData(book_idx);

    if (!book) break;

    PicturePtr picture =
        Picture::Make(book->coverDim, (uint8_t *)book->coverBitmap, book->coverSize());

    page->putPicture(std::move(picture),
                     Pos(xpos + ((BooksDir::coverDim.width - book->coverDim.width) >> 1),
                         ypos + ((BooksDir::coverDim.height - book->coverDim.height) >> 1)));

    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      if (item_idx == currentItemIdx) {
        page->putHighlight(Dim(BooksDir::coverDim.width + 4, BooksDir::coverDim.height + 4),
                           Pos(xpos - 2, ypos - 2));
        page->putHighlight(Dim(BooksDir::coverDim.width + 6, BooksDir::coverDim.height + 6),
                           Pos(xpos - 3, ypos - 3));

        fmt.fontIndex = TITLE_FONT;
        fmt.fontSize   = TITLE_FONT_SIZE;
        fmt.fontStyle = Fonts::FaceStyle::NORMAL;

        char title[MAX_TITLE_SIZE];
        title[MAX_TITLE_SIZE - 1] = 0;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstringop-truncation"
        strncpy(title, book->title, MAX_TITLE_SIZE - 1);
        if (strlen(book->title) > (MAX_TITLE_SIZE - 1)) {
          strcpy(&title[MAX_TITLE_SIZE - 5], " ...");
        }
        #pragma GCC diagnostic pop

        page->setLimits(fmt);
        page->newParagraph(fmt);
        #if EPUB_INKPLATE_BUILD
          if (nvsMgr.idExists(book->id)) page->addText("[Reading] ", fmt);
        #endif
        page->addText(title, fmt);
        page->endParagraph(fmt);

        fmt.fontIndex = AUTHOR_FONT;
        fmt.fontSize   = AUTHOR_FONT_SIZE;
        fmt.fontStyle = Fonts::FaceStyle::ITALIC;

        page->newParagraph(fmt);
        page->addText(book->author, fmt);
        page->endParagraph(fmt);
      }
    #endif

    line_pos++;

    if (line_pos >= lineCount) {
      xpos += BooksDir::coverDim.width + horizSpaceBetweenEntries;
      ypos     = firstEntryYpos;
      line_pos = 0;
    } else {
      ypos += BooksDir::coverDim.height + vertSpaceBetweenEntries;
    }
  }

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    fmt.screenTop = 18;
    page->setLimits(fmt);
    page->newParagraph(fmt);
    page->addText(TOUCH_AND_HOLD_STR, fmt);
    page->endParagraph(fmt);
  #endif

  page->putRounded(Dim(Screen::getWidth() - 20, firstEntryYpos - 20), Pos(10, 10));

  ScreenBottom::show(page, currentPageNbr, pageCount);

  page->paint();
}

auto MatrixBooksDirViewer::highlight(int16_t item_idx) -> void {
  int16_t book_idx, column_idx, line_idx, xpos, ypos;

  BooksDir::EBookRecordPtr book;

  Page::Format fmt = {
      .lineHeightFactor = 0.9,
      .fontIndex         = TITLE_FONT,
      .fontSize           = TITLE_FONT_SIZE,
      .screenLeft        = 15,
      .screenRight       = 15,
      .screenTop         = 18,
      .screenBottom      = 100,
      .align              = CSS::Align::CENTER,
  };

  page->setComputeMode(Page::ComputeMode::DISPLAY);

  page->start(fmt);

  if ((currentItemIdx != -1) && (currentItemIdx != item_idx)) {

    // Clear the highlighting of the current item

    book_idx = currentPageNbr * booksPerPage + currentItemIdx;

    column_idx = currentItemIdx / lineCount;
    line_idx   = currentItemIdx % lineCount;

    xpos = 5 + ((BooksDir::coverDim.width + horizSpaceBetweenEntries) * column_idx);
    ypos = firstEntryYpos + ((BooksDir::coverDim.height + vertSpaceBetweenEntries) * line_idx);

    book = booksDir.getBookData(book_idx);

    if (!book) return;

    // Font * font = fonts.get(1, 9);

    page->clearHighlight(Dim(BooksDir::coverDim.width + 4, BooksDir::coverDim.height + 4),
                         Pos(xpos - 2, ypos - 2));
    page->clearHighlight(Dim(BooksDir::coverDim.width + 6, BooksDir::coverDim.height + 6),
                         Pos(xpos - 3, ypos - 3));

    page->clearRegion(Dim(Screen::getWidth() - 40, (titleFontHeight << 1) + authorFontHeight),
                      Pos(20, 20));
  }
  // Highlight the new current item

  currentItemIdx = -1;

  book_idx = currentPageNbr * booksPerPage + item_idx;

  book = booksDir.getBookData(book_idx);

  if (!book) return;

  currentItemIdx = item_idx;

  column_idx = currentItemIdx / lineCount;
  line_idx   = currentItemIdx % lineCount;

  xpos = 5 + ((BooksDir::coverDim.width + horizSpaceBetweenEntries) * column_idx);
  ypos = firstEntryYpos + ((BooksDir::coverDim.height + vertSpaceBetweenEntries) * line_idx);

  page->putHighlight(Dim(BooksDir::coverDim.width + 4, BooksDir::coverDim.height + 4),
                     Pos(xpos - 2, ypos - 2));
  page->putHighlight(Dim(BooksDir::coverDim.width + 6, BooksDir::coverDim.height + 6),
                     Pos(xpos - 3, ypos - 3));

  page->clearRegion(Dim(Screen::getWidth() - 40, (titleFontHeight << 1) + authorFontHeight),
                    Pos(20, 20));

  fmt.fontIndex = TITLE_FONT;
  fmt.fontSize   = TITLE_FONT_SIZE;
  fmt.fontStyle = Fonts::FaceStyle::NORMAL;

  char title[MAX_TITLE_SIZE];
  title[MAX_TITLE_SIZE - 1] = 0;

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstringop-truncation"
  strncpy(title, book->title, MAX_TITLE_SIZE - 1);
  if (strlen(book->title) > (MAX_TITLE_SIZE - 1)) {
    strcpy(&title[MAX_TITLE_SIZE - 5], " ...");
  }
  #pragma GCC diagnostic pop

  page->setLimits(fmt);
  page->newParagraph(fmt);
  #if EPUB_INKPLATE_BUILD
    if (nvsMgr.idExists(book->id)) page->addText("[Reading] ", fmt);
  #endif
  page->addText(title, fmt);
  page->endParagraph(fmt);

  fmt.fontIndex = AUTHOR_FONT;
  fmt.fontSize   = AUTHOR_FONT_SIZE;
  fmt.fontStyle = Fonts::FaceStyle::ITALIC;

  page->newParagraph(fmt);
  page->addText(book->author, fmt);
  page->endParagraph(fmt);

  ScreenBottom::show(page, currentPageNbr, pageCount);

  page->paint(false);
}

auto MatrixBooksDirViewer::clearHighlight() -> void {
  if (currentItemIdx == -1) return;

  page->setComputeMode(Page::ComputeMode::DISPLAY);

  // Clear the highlighting of the current item

  int16_t book_idx = currentPageNbr * booksPerPage + currentItemIdx;

  int16_t column_idx = currentItemIdx / lineCount;
  int16_t line_idx   = currentItemIdx % lineCount;

  int16_t xpos = 5 + ((BooksDir::coverDim.width + horizSpaceBetweenEntries) * column_idx);
  int16_t ypos =
      firstEntryYpos + ((BooksDir::coverDim.height + vertSpaceBetweenEntries) * line_idx);

  auto book = booksDir.getBookData(book_idx);

  if (!book) return;

  // Font * font = fonts.get(1, 9);

  Page::Format fmt = {
      .lineHeightFactor = 0.9,
      .fontIndex         = TITLE_FONT,
      .fontSize           = TITLE_FONT_SIZE,
      .screenBottom =
          static_cast<uint16_t>(Screen::getHeight() - (ypos + BooksDir::coverDim.height + 20)),
  };

  page->start(fmt);

  page->clearHighlight(Dim(BooksDir::coverDim.width + 4, BooksDir::coverDim.height + 4),
                       Pos(xpos - 2, ypos - 2));
  page->clearHighlight(Dim(BooksDir::coverDim.width + 6, BooksDir::coverDim.height + 6),
                       Pos(xpos - 3, ypos - 3));

  page->clearRegion(Dim(Screen::getWidth() - 40, (titleFontHeight << 1) + authorFontHeight),
                    Pos(20, 20));

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    fmt.screenTop = 18;
    page->setLimits(fmt);
    page->newParagraph(fmt);
    page->addText(TOUCH_AND_HOLD_STR, fmt);
    page->endParagraph(fmt);
  #endif

  #if EPUB_INKPLATE_BUILD
    BatteryViewer::show(page);
  #endif

  page->paint(false);

  currentItemIdx = -1;
}

auto MatrixBooksDirViewer::showPageAndHighlight(int16_t book_idx) -> int16_t {
  int16_t page_nbr = book_idx / booksPerPage;
  int16_t item_idx = book_idx % booksPerPage;

  if (currentPageNbr != page_nbr) {
    showPage(page_nbr, item_idx);
    currentPageNbr = page_nbr;
  } else {
    if (item_idx != currentItemIdx) highlight(item_idx);
  }
  currentBookIdx = book_idx;
  return currentBookIdx;
}

auto MatrixBooksDirViewer::highlightBook(int16_t book_idx) -> void { highlight(book_idx % booksPerPage); }

auto MatrixBooksDirViewer::nextPage() -> int16_t {
  int16_t page_nbr = currentPageNbr + 1;
  if (page_nbr >= pageCount) {
    page_nbr = pageCount - 1;
  }
  if (currentPageNbr != page_nbr) {
    showPage(page_nbr, 0);
    currentBookIdx = page_nbr * booksPerPage;
  } else if ((page_nbr + 1) == pageCount) {
    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      highlight(booksDir.getBookCount() % booksPerPage - 1);
    #endif
    currentBookIdx = booksDir.getBookCount() - 1;
  }
  return currentBookIdx;
}

auto MatrixBooksDirViewer::prevPage() -> int16_t {
  int16_t page_nbr = currentPageNbr - 1;
  if (page_nbr < 0) {
    page_nbr = 0;
  }
  if (currentPageNbr != page_nbr) {
    showPage(page_nbr, 0);
  } else {
    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      highlight(0);
    #endif
  }
  currentBookIdx = page_nbr * booksPerPage;
  return currentBookIdx;
}

auto MatrixBooksDirViewer::nextItem() -> int16_t {
  int16_t book_idx = currentBookIdx + 1;
  if (book_idx >= booksDir.getBookCount()) {
    book_idx = booksDir.getBookCount() - 1;
  }
  return showPageAndHighlight(book_idx);
}

auto MatrixBooksDirViewer::prevItem() -> int16_t {
  int16_t book_idx = currentBookIdx - 1;
  if (book_idx < 0) book_idx = 0;
  return showPageAndHighlight(book_idx);
}

auto MatrixBooksDirViewer::nextColumn() -> int16_t {
  int16_t book_idx = currentBookIdx + lineCount;
  if (book_idx >= booksDir.getBookCount()) {
    book_idx = booksDir.getBookCount() - 1;
  }
  return showPageAndHighlight(book_idx);
}

auto MatrixBooksDirViewer::prevColumn() -> int16_t {
  int16_t book_idx = currentBookIdx - lineCount;
  if (book_idx < 0) book_idx = 0;
  return showPageAndHighlight(book_idx);
}
