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
  #include "nvs.h"
#endif

void BookController::enter() {
  LOG_D("===> Enter()...");

  // When entering the book viewer, the BookController::openBook() method was already called,
  // the epub instance is ready and pointing at the required book.

  bookViewer = BookViewer::Make();

  // As the page formatting options may have changed since the last time the book was requested to
  // be shown, we need to check if the page locations are still valid. If not, we need to
  // recalculate them before showing the current page.

  pageLocs.checkForFormatChanges(epub, currentPageId.itemrefIndex);

  const PageId *id = pageLocs.getPageId(currentPageId);
  if (id != nullptr) {
    currentPageId.itemrefIndex = id->itemrefIndex;
    currentPageId.offset       = id->offset;
  } else {
    currentPageId.itemrefIndex = 0;
    currentPageId.offset       = 0;
  }
  bookViewer->showPage(currentPageId, epub);
}

void BookController::leave(bool goingToDeepSleep) {
  LOG_D("===> leave()...");

  booksDirController.saveLastBook(currentPageId, goingToDeepSleep);
  bookViewer.reset();
}

auto BookController::openBook(const std::string &bookTitle, const std::string &bookFilename,
                              const PageId &pageId) -> bool {
  LOG_D("===> openBook()...");

  MsgViewer::show(MsgViewer::MsgType::BOOK, false, false, "Loading a book",
                  "The book \" %s \" is loading. Please wait.", bookTitle.c_str());

  bool newDocument = !epub || (bookFilename != epub->getCurrentFilename());

  if (newDocument) {
    // In case we are already in the middle of processing another book, we need to
    // stop it before starting a new one.
    pageLocs.stopDocument();

    epub = EPub::Make();

    if (epub->open(bookFilename)) {
      pageLocs.startNewDocument(epub, pageId.itemrefIndex);
    } else {
      MsgViewer::show(MsgViewer::MsgType::ALERT, true, true, "Error",
                      "An error occurred while opening the book. Please try again.");
      return false;
    }
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
  //     // bookViewer->showPage(currentPageId);
  //     return true;
  //   }
  // }

  return true;
}

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  void BookController::inputEvent(const EventMgr::Event &event) {
    const PageId *pageId;
    switch (event.kind) {
    case EventMgr::EventKind::SWIPE_RIGHT:
      if (event.y < (Screen::getHeight() - 40)) {
        pageId = pageLocs.getPrevPageId(currentPageId);
        if (pageId != nullptr) {
          currentPageId.itemrefIndex = pageId->itemrefIndex;
          currentPageId.offset       = pageId->offset;
          bookViewer->showPage(currentPageId, epub);
        }
      } else {
        pageId = pageLocs.getPrevPageId(currentPageId, 10);
        if (pageId != nullptr) {
          currentPageId.itemrefIndex = pageId->itemrefIndex;
          currentPageId.offset       = pageId->offset;
          bookViewer->showPage(currentPageId, epub);
        }
      }
      break;

    case EventMgr::EventKind::SWIPE_LEFT:
      if (event.y < (Screen::getHeight() - 40)) {
        pageId = pageLocs.getNextPageId(currentPageId);
        if (pageId != nullptr) {
          currentPageId.itemrefIndex = pageId->itemrefIndex;
          currentPageId.offset       = pageId->offset;
          bookViewer->showPage(currentPageId, epub);
        }
      } else {
        pageId = pageLocs.getNextPageId(currentPageId, 10);
        if (pageId != nullptr) {
          currentPageId.itemrefIndex = pageId->itemrefIndex;
          currentPageId.offset       = pageId->offset;
          bookViewer->showPage(currentPageId, epub);
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
            bookViewer->showPage(currentPageId, epub);
          }
        } else if (event.x > ((Screen::getWidth() / 3) * 2)) {
          pageId = pageLocs.getNextPageId(currentPageId);
          if (pageId != nullptr) {
            currentPageId.itemrefIndex = pageId->itemrefIndex;
            currentPageId.offset       = pageId->offset;
            bookViewer->showPage(currentPageId, epub);
          }
        } else {
          bookParamController.setOwnershipOfBook(epub);
          appController.setController(AppController::Ctrl::PARAM);
        }
      } else {
        bookParamController.setOwnershipOfBook(epub);
        appController.setController(AppController::Ctrl::PARAM);
      }
      break;

    default:
      break;
    }
  }
#else
  void BookController::inputEvent(const EventMgr::Event &event) {
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
        bookViewer->showPage(currentPageId, epub);
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
        bookViewer->showPage(currentPageId, epub);
      }
      break;

      #if EXTENDED_CASE
      case EventMgr::EventKind::DBL_NEXT:
      #else
      case EventMgr::EventKind::NEXT:
      #endif
      pageId = pageLocs.getNextPageId(currentPageId);
      if (pageId != nullptr) {
        currentPageId.itemrefIndex = pageId->itemrefIndex;
        currentPageId.offset       = pageId->offset;
        bookViewer->showPage(currentPageId, epub);
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
        bookViewer->showPage(currentPageId, epub);
      }
      break;

    case EventMgr::EventKind::SELECT:
    case EventMgr::EventKind::DBL_SELECT:
      bookParamController.setOwnershipOfBook(epub);
      appController.setController(AppController::Ctrl::PARAM);
      break;

    case EventMgr::EventKind::NONE:
      break;
    }
  }
#endif
