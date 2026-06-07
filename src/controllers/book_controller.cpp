// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_CONTROLLER__ 1
#include "controllers/book_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/book_param_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "models/epub.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/page.hpp"

#if EPUB_INKPLATE_BUILD
  #include "book_controller.hpp"
  #include "nvs.h"
#endif

auto BookController::enter() -> void {
  LOG_D("===> Enter()...");

  // When entering the book viewer, the BookController::openBook() method was already called,
  // the epub instance is ready and pointing at the required book.

  bookViewer = BookViewer::Make(epub->getFonts());

  // As the page formatting options may have changed since the last time the book was requested to
  // be shown, we need to check if the page locations are still valid. If not, we need to
  // recalculate them before showing the current page.

  pageLocs.checkForFormatChanges(epub, currentPageId.itemrefIndex);

  readyForDisplayPageId.reset();

  const PageId *id = pageLocs.getPageId(currentPageId);
  if (id != nullptr) {
    currentPageId.itemrefIndex = id->itemrefIndex;
    currentPageId.offset       = id->offset;
    // LOG_I("Showing page: itemrefIndex={} offset={}", currentPageId.itemrefIndex,
    //       currentPageId.offset);
  } else {
    // Keep the last known reading position while page locations are recomputed.
    // Falling back to {0,0} forces the cover page and creates UI glitches.
    LOG_W("PageLocs not ready for item={} offset={}, keeping current page id",
          currentPageId.itemrefIndex, currentPageId.offset);
  }

  if ((currentPageId.itemrefIndex >= 0) && (currentPageId.offset >= 0)) {
    showPage(currentPageId, epub);
  } else {
    LOG_I("Skipping showPage while waiting for valid PageId: itemref={} offset={}",
          currentPageId.itemrefIndex, currentPageId.offset);
  }
}

auto BookController::leave(bool goingToDeepSleep) -> void {
  LOG_D("===> leave()...");

  booksDirController.saveLastBook(currentPageId, goingToDeepSleep);
  bookViewer.reset();
}

auto BookController::openBook(const HimemString &bookTitle, const HimemString &bookFilename,
                              const PageId &pageId) -> bool {
  LOG_D("===> openBook()...");

  MsgViewer::show(MsgViewer::MsgType::BOOK, false, false, "Loading a book",
                  "The book \" %s \" is loading. Please wait.", bookTitle.c_str());

  bool newDocument = !epub || (bookFilename != epub->getCurrentFilename());

  if (newDocument) {

    // In case we are already in the middle of processing another book, we need to
    // stop it before starting a new one. If PageLocs is still processing the previous
    // book, we need to stop it before starting a new one. Also, the BookParamController
    // may be still owning the previous book, so we need to reset it as well.

    pageLocs.stopControlTask();
    bookParamController.becomeOwnerOfBook(nullptr);

    epub = EPub::Make();

    if (epub->open(bookFilename)) {
      pageLocs.startNewDocument(epub, pageId.itemrefIndex);
      currentPageId = pageId;
    } else {
      MsgViewer::show(MsgViewer::MsgType::ALERT, true, true, "Error",
                      "An error occurred while opening the book. Please try again.");
      return false;
    }
  } else {
    // Required as the screen resolution may have changed since the last time the book was shown,
    // and the cached fonts may be too large for the new resolution. They will be reloaded when needed.
    epub->getFonts().clear();
  }

  // if (epub->open(book_filename)) {
  //   if (newDocument) {
  //     pageLocs.startNewDocument(epub->get_item_count(), pageId.itemrefIndex);
  //   } else {
  //     pageLocs.checkForFormatChanges(epub, pageId.itemrefIndex);
  //   }
  //   const PageId *id = pageLocs.getPageId(pageId);
  //   if (id != nullptr) {
  //     currentPageId.itemrefIndex = id->itemrefIndex;
  //     currentPageId.offset        = id->offset;
  //     // showPage(currentPageId);
  //     return true;
  //   }
  // }

  return true;
}

/**
 * @brief Show a page given its ID.
 *
 * This method is called when a new page needs to be shown.
 * It first checks if the page is already prepared and ready for display. If it is, it directly
 * displays it. If the page is not ready, it tries to prepare it. If the preparation is successful,
 * it then displays the page. After displaying the page, it also tries to prepare the next page in
 * the background, so that if the user goes to the next page, it can be displayed more quickly. If
 * the next page cannot be prepared, it resets the readyForDisplayPageId to an invalid value.
 */
auto BookController::showPage(const PageId &pageId, EPubPtr &epub) -> void {
  if (bookViewer != nullptr) {
    if ((pageId == readyForDisplayPageId) || bookViewer->preparePage(pageId, epub)) {
      bookViewer->displayPage(pageId);
      auto nextPageId = pageLocs.getNextPageId(currentPageId);
      if ((nextPageId != nullptr) && !nextPageId->firstPage() &&
          bookViewer->preparePage(*nextPageId, epub)) {
        readyForDisplayPageId = *nextPageId;
      } else {
        readyForDisplayPageId = PageId{ -1, -1 };
      }
    }
  }
}

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  auto BookController::inputEvent(const EventMgr::Event &event) -> void {

    // LOG_I("inputEvent: kind={} x={} y={}", static_cast<int>(event.kind), event.x, event.y);

    const PageId *pageId;
    switch (event.kind) {
    case EventMgr::EventKind::SWIPE_RIGHT:
      if (event.y < (Screen::getHeight() - 40)) {
        pageId = pageLocs.getPrevPageId(currentPageId);
        if (pageId != nullptr) {
          currentPageId.itemrefIndex = pageId->itemrefIndex;
          currentPageId.offset       = pageId->offset;
          showPage(currentPageId, epub);
        }
      } else {
        pageId = pageLocs.getPrevPageId(currentPageId, 10);
        if (pageId != nullptr) {
          currentPageId.itemrefIndex = pageId->itemrefIndex;
          currentPageId.offset       = pageId->offset;
          showPage(currentPageId, epub);
        }
      }
      break;

    case EventMgr::EventKind::SWIPE_LEFT:
      if (event.y < (Screen::getHeight() - 40)) {
        pageId = pageLocs.getNextPageId(currentPageId);
        if (pageId != nullptr) {
          currentPageId.itemrefIndex = pageId->itemrefIndex;
          currentPageId.offset       = pageId->offset;
          showPage(currentPageId, epub);
        }
      } else {
        pageId = pageLocs.getNextPageId(currentPageId, 10);
        if (pageId != nullptr) {
          currentPageId.itemrefIndex = pageId->itemrefIndex;
          currentPageId.offset       = pageId->offset;
          showPage(currentPageId, epub);
        }
      }
      break;

    case EventMgr::EventKind::TAP:
      if (event.y < (Screen::getHeight() - 40)) {
        if (event.x < (Screen::getWidth() / 3)) {
          pageId = pageLocs.getPrevPageId(currentPageId);
          if (pageId != nullptr) {
            currentPageId.itemrefIndex = pageId->itemrefIndex;
            currentPageId.offset       = pageId->offset;
            showPage(currentPageId, epub);
          }
        } else if (event.x > ((Screen::getWidth() / 3) * 2)) {
          pageId = pageLocs.getNextPageId(currentPageId);
          if (pageId != nullptr) {
            currentPageId.itemrefIndex = pageId->itemrefIndex;
            currentPageId.offset       = pageId->offset;
            showPage(currentPageId, epub);
          }
        } else {
          bookParamController.becomeOwnerOfBook(std::move(epub));
          appController.setController(AppController::Ctrl::PARAM);
        }
      } else {
        bookParamController.becomeOwnerOfBook(std::move(epub));
        appController.setController(AppController::Ctrl::PARAM);
      }
      break;

    default:
      break;
    }
  }
#else
  auto BookController::inputEvent(const EventMgr::Event &event) -> void {
    const PageId *pageId;
    switch (event.kind) {
    #if EXTENDED_CASE
      case EventMgr::EventKind::DBL_PREV:
    #else
      case EventMgr::EventKind::PREV:
        #endif
        pageId = pageLocs.getPrevPageId(currentPageId);
        if (pageId != nullptr) {
          currentPageId.itemrefIndex = pageId->itemrefIndex;
          currentPageId.offset       = pageId->offset;
          showPage(currentPageId, epub);
        }
        break;

        #if EXTENDED_CASE
          case EventMgr::EventKind::PREV:
        #else
          case EventMgr::EventKind::DBL_PREV:
            #endif
            pageId = pageLocs.getPrevPageId(currentPageId, 10);
            if (pageId != nullptr) {
              currentPageId.itemrefIndex = pageId->itemrefIndex;
              currentPageId.offset       = pageId->offset;
              showPage(currentPageId, epub);
            }
            break;

            #if EXTENDED_CASE
              case EventMgr::EventKind::DBL_NEXT:
            #else
              case EventMgr::EventKind::SELECT:
              case EventMgr::EventKind::NEXT:
                #endif
                pageId = pageLocs.getNextPageId(currentPageId);
                if (pageId != nullptr) {
                  currentPageId.itemrefIndex = pageId->itemrefIndex;
                  currentPageId.offset       = pageId->offset;
                  showPage(currentPageId, epub);
                }
                break;

                #if EXTENDED_CASE
                  case EventMgr::EventKind::NEXT:
                #else
                  case EventMgr::EventKind::DBL_NEXT:
                    #endif
                    pageId = pageLocs.getNextPageId(currentPageId, 10);
                    if (pageId != nullptr) {
                      currentPageId.itemrefIndex = pageId->itemrefIndex;
                      currentPageId.offset       = pageId->offset;
                      showPage(currentPageId, epub);
                    }
                    break;

                  case EventMgr::EventKind::DBL_SELECT:
                    bookParamController.becomeOwnerOfBook(std::move(epub));
                    appController.setController(AppController::Ctrl::PARAM);
                    break;

                  case EventMgr::EventKind::NONE:
                    break;
    }
  }
#endif
