// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_VIEWER__ 1
#include "viewers/book_viewer.hpp"
#include "controllers/book_controller.hpp"

#include "config.hpp"
#include "fonts.hpp"
#include "helpers/picture_load_icon.hpp"
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
#include <memory>
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

auto BookViewer::recreatePage(Fonts &fonts) -> bool {
  page.reset();
  page = Page::Make(fonts);
  if (page == nullptr) {
    LOG_E("Unable to allocate a new Page instance");
    return false;
  }
  return true;
}

auto BookViewer::buildPageAt(const PageId &pageId, EPubPtr &epub) -> void {
  LOG_D("buildPageAt()");

  FontPtr &font = epub->getFonts().getFont(ScreenBottom::FONT);
  pageBottom    = font->getCharsHeight(ScreenBottom::FONT_SIZE) + 15;

  if (epub->getItemAtIndex(pageId.itemrefIndex)) {

    int16_t idx;

    int8_t showTitle = 0;
    config.get(Config::Ident::SHOW_TITLE, &showTitle);

    uint16_t page_top             = 0;
    int16_t title_baseline_offset = 0;

    if (showTitle != 0) {
      FontPtr &title_font   = epub->getFonts().getFont(TITLE_FONT);
      page_top              = title_font->getCharsHeight(TITLE_FONT_SIZE) + 10;
      title_baseline_offset = page_top + title_font->getDescenderHeight(TITLE_FONT_SIZE);
    }

    if ((idx = epub->getFonts().getFontIndex("Fontbase", FaceStyle::NORMAL)) == -1) {
      idx = 3;
    }

    Page::Format fmt = {
        .lineHeightFactor = epub->getLineHeightFactor(),
        .fontIndex        = idx,
        .fontSize         = epub->getBookFormatParams()->fontSize,
        .screenTop        = page_top,
        .screenBottom     = pageBottom,
    };

    std::this_thread::yield();
    const PageLocs::PageInfo *page_info = pageLocs.getPageInfo(pageId);

    if (page_info == nullptr) return;

    auto dom    = DOM::Make(epub->getDomPools());
    auto interp = std::make_unique<BookViewerInterp>(epub, page, dom, Page::ComputeMode::DISPLAY,
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

      Page::Format *new_fmt = interp->duplicateFmt(fmt);

      if (interp->buildPagesRecurse(node, *new_fmt, dom->body, 1)) {

        if (page->someDataWaiting()) page->endParagraph(fmt);

        fmt.lineHeightFactor = 1.0;
        fmt.fontIndex        = TITLE_FONT;
        fmt.fontSize         = TITLE_FONT_SIZE;
        fmt.fontStyle        = FaceStyle::ITALIC;
        fmt.align            = CSS::Align::CENTER;

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

        ScreenBottom::show(page, pageLocs.getPageNbr(pageId), pageLocs.getPageCountOrPercent());

        page->paint();
      }
      interp->showStat();
      interp->releaseFmt(new_fmt);
    }

    interp->checkForCompletion();
  }
  LOG_D("end of buildPageAt()");
}

auto BookViewer::showFakeCover(EPubPtr &epub) -> void {
  Page::Format fmt = {
      .fontIndex    = SYSTEM_ITALIC_FONT_INDEX,
      .fontSize     = 14,
      .screenTop    = 100,
      .screenBottom = 30,
      .fontStyle    = FaceStyle::ITALIC,
      .align        = CSS::Align::CENTER,
  };

  std::string title  = epub->getTitle();
  std::string author = epub->getAuthor();

  page->start(fmt);

  page->newParagraph(fmt, false);
  page->addText(author, fmt);
  page->endParagraph(fmt);

  fmt.fontIndex = SYSTEM_REGULAR_FONT_INDEX;
  fmt.fontSize  = 18;
  fmt.screenTop = 200;
  fmt.fontStyle = FaceStyle::NORMAL;

  page->setLimits(fmt);
  page->newParagraph(fmt, false);
  page->addText(title, fmt);
  page->endParagraph(fmt);

  page->paint();
}

auto BookViewer::showPage(const PageId &pageId, EPubPtr &epub) -> void {
  if ((pageId.itemrefIndex < 0) || (pageId.offset < 0)) {
    LOG_W("Ignoring invalid showPage request: itemref=%d offset=%" PRIi32, pageId.itemrefIndex,
          pageId.offset);
    return;
  }

  if (!recreatePage(epub->getFonts())) return;

  current_page_id = pageId;

  const bool isCoverPage = (pageId.itemrefIndex == 0) && (pageId.offset == 0);

  if (isCoverPage) {
    if (epub->getBookFormatParams()->showPictures != 0) {
      HimemString fname = epub->getCoverFilename();
      if (!fname.empty()) {
        // LOG_D("Cover filename: %s", fname);
        auto coverInfo = epub->getPicture(fname, false);
        if (coverInfo != nullptr) {
          showPictureLoadIcon(coverInfo->getDim());
        }

        auto pict = epub->getPicture(fname, true);

        if (pict != nullptr) {
          if (!page->showCover(pict)) {
            LOG_W("Cover picture display failed, falling back to generated cover");
            showFakeCover(epub);
          }
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
