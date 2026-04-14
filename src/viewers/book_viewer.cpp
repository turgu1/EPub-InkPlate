// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_VIEWER__ 1
#include "viewers/book_viewer.hpp"
#include "controllers/book_controller.hpp"

#include "models/config.hpp"
#include "picture_factory.hpp"
#include "viewers/html_interpreter.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/screen_bottom.hpp"

#if EPUB_INKPLATE_BUILD
  #include "viewers/battery_viewer.hpp"
#endif

#include "alloc.hpp"
#include "screen.hpp"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "logging.hpp"

using namespace pugi;

class BookViewerInterp : public HTMLInterpreter {
public:
  BookViewerInterp(EPubPtr &the_epub, PagePtr &the_page, DOMPtr &the_dom,
                   Page::ComputeMode the_comp_mode, const EPub::ItemInfo &the_item)
      : HTMLInterpreter(the_epub, the_page, the_dom, the_comp_mode, the_item) {}
  ~BookViewerInterp() {}

protected:
  auto pageEnd(const Page::Format &fmt) -> bool {
    LOG_D("---- PAGE END ----");
    return true;
  }
};

auto BookViewer::buildPageAt(const PageId &pageId, EPubPtr &epub) -> void {
  LOG_D("buildPageAt()");
  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif

  Font *font = fonts.get(ScreenBottom::FONT);
  pageBottom = font->getCharsHeight(ScreenBottom::FONT_SIZE) + 15;

  // page->setComputeMode(Page::ComputeMode::MOVE);

  // showPictures = epub->getBookFormatParams()->showPictures != 0;

  if (epub->getItemAtIndex(pageId.itemrefIndex)) {

    int16_t idx;

    int8_t showTitle = 0;
    config.get(Config::Ident::SHOW_TITLE, &showTitle);

    uint16_t page_top             = 0;
    int16_t title_baseline_offset = 0;

    if (showTitle != 0) {
      Font *title_font      = fonts.get(TITLE_FONT);
      page_top              = title_font->getCharsHeight(TITLE_FONT_SIZE) + 10;
      title_baseline_offset = page_top + title_font->getDescenderHeight(TITLE_FONT_SIZE);
    }

    if ((idx = fonts.getIndex("Fontbase", Fonts::FaceStyle::NORMAL)) == -1) {
      idx = 3;
    }

    int8_t fontSize = epub->getBookFormatParams()->fontSize;

    Page::Format fmt = {
        .lineHeightFactor = 0.95,
        .fontIndex         = idx,
        .fontSize           = fontSize,
        .screenTop         = page_top,
        .screenBottom      = pageBottom,
    };

    mutex.unlock();
    std::this_thread::yield();
    const PageLocs::PageInfo *page_info = pageLocs.getPageInfo(pageId);
    mutex.lock();

    if (page_info == nullptr) return;

    // currentOffset       = 0;
    // start_of_page_offset = pageId.offset;
    // end_of_page_offset   = pageId.offset + page_info->size;

    auto dom                 = DOM::Make();
    BookViewerInterp *interp = new BookViewerInterp(epub, page, dom, Page::ComputeMode::DISPLAY,
                                                    epub->getCurrentItemInfo());
    interp->setLimits(pageId.offset, pageId.offset + page_info->size,
                      epub->getBookFormatParams()->showPictures != 0);

    #if DEBUGGING_AID
      interp->setPagesToShowState(PAGE_FROM, PAGE_TO);
      interp->checkPageToShow(pageLocs.getPageNbr(pageId));
    #endif

    xml_node node;

    if ((node = epub->getCurrentItem().child("html").child("body"))) {

      page->start(fmt);

      // #if EPUB_INKPLATE_BUILD
      //   esp_task_wdt_reset();
      // #endif

      Page::Format *new_fmt = interp->duplicateFmt(fmt);

      if (interp->buildPagesRecurse(node, *new_fmt, dom->body, 1)) {

        if (page->someDataWaiting()) page->endParagraph(fmt);

        // TTF * font = fonts.get(0, 7);

        fmt.lineHeightFactor = 1.0;
        fmt.fontIndex         = TITLE_FONT;
        fmt.fontSize           = TITLE_FONT_SIZE;
        fmt.fontStyle         = Fonts::FaceStyle::ITALIC;
        fmt.align              = CSS::Align::CENTER;

        std::ostringstream ostr;

        if (showTitle != 0) {
          const char *t = epub->getTitle();
          if (strlen(t) > 50) {
            // Only the first 50 characters of the title will be shown
            char str[55];
            strncpy(str, t, 50);
            str[50] = 0;
            strcat(str, "...");
            ostr << str;
          } else {
            ostr << t;
          }
          // page->putHighlight(Dim(screen.getWidth(), title_baseline_offset), Pos(0, 0));
          page->putStrAt(ostr.str(), Pos(Page::HORIZONTAL_CENTER, title_baseline_offset), fmt);
        }

        ScreenBottom::show(page, pageLocs.getPageNbr(pageId), pageLocs.getPageCount());

        page->paint();
      }
      interp->showStat();
      interp->releaseFmt(new_fmt);
    }

    interp->checkForCompletion();

    delete interp;
    interp = nullptr;
  }
  LOG_D("end of buildPageAt()");
  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif
}

auto BookViewer::showFakeCover(EPubPtr &epub) -> void {
  Page::Format fmt = {
      .fontIndex    = 2,
      .fontSize      = 14,
      .screenTop    = 100,
      .screenBottom = 30,
      .fontStyle    = Fonts::FaceStyle::ITALIC,
      .align         = CSS::Align::CENTER,
  };

  std::string title  = epub->getTitle();
  std::string author = epub->getAuthor();

  page->start(fmt);

  page->newParagraph(fmt, false);
  page->addText(author, fmt);
  page->endParagraph(fmt);

  fmt.fontIndex = 1;
  fmt.fontSize   = 18;
  fmt.screenTop = 200;
  fmt.fontStyle = Fonts::FaceStyle::NORMAL;

  page->setLimits(fmt);
  page->newParagraph(fmt, false);
  page->addText(title, fmt);
  page->endParagraph(fmt);

  page->paint();
}

auto BookViewer::showPage(const PageId &pageId, EPubPtr &epub) -> void {
  std::scoped_lock guard(mutex);

  current_page_id = pageId;

  // if (pageLocs.getPageNbr(pageId) == 0) {
  if ((pageId.itemrefIndex == 0) && (pageId.offset == 0)) {

    if (epub->getBookFormatParams()->showPictures != 0) {
      std::string fname = epub->getCoverFilename();
      if (!fname.empty()) {
        // LOG_D("Cover filename: %s", fname);
        auto pict = epub->getPicture(fname, true);

        if (pict != nullptr) {
          page->showCover(pict);
        } else {
          LOG_D("Unable to retrieve cover file: %s", fname.c_str());
          showFakeCover(epub);
        }
      } else {
        LOG_D("It seems there is no cover.");
        buildPageAt(pageId, epub);
      }
    } else {
      showFakeCover(epub);
    }
  } else {
    buildPageAt(pageId, epub);
  }
}
