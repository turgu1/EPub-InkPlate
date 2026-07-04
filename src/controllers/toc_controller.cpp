// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __TOC_CONTROLLER__ 1
#include "controllers/toc_controller.hpp"

#include "config.hpp"
#include "controllers/app_controller.hpp"
#include "controllers/book_controller.hpp"
#include "controllers/book_param_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "models/books_dir.hpp"
#include "models/toc.hpp"

#if EPUB_INKPLATE_BUILD
  #include "nvs.h"
#endif

#include <string>

auto TocController::enter() -> void {
  if (!epub) {
    MsgViewer::show(MsgViewer::MsgType::ALERT, false, false, "Error",
                    "No book is loaded. Cannot display the table of content.");
    bookParamController.becomeOwnerOfBook(std::move(epub));
    appController.setController(AppController::Ctrl::PARAM);
    return;
  }

  if (epub->toc == nullptr) {
    epub->toc = TOC::Make();
    if (epub->toc && !(epub->toc->load(epub) && epub->toc->isReady())) {
      MsgViewer::show(MsgViewer::MsgType::ALERT, false, false, "Error",
                      "The table of content for the current book is not available yet.");
      bookParamController.becomeOwnerOfBook(std::move(epub));
      appController.setController(AppController::Ctrl::PARAM);
      return;
    }
  }

  tocViewer = TocViewer::Make(epub);
  if (!tocViewer) {
    MsgViewer::show(MsgViewer::MsgType::ALERT, false, false, "Error",
                    "Unable to display the table of content for the current book.");
    bookParamController.becomeOwnerOfBook(std::move(epub));
    appController.setController(AppController::Ctrl::PARAM);
    return;
  }

  tocViewer->setup();

  if ((currentEntryIndex == -1) || (currentBookIndex != booksDirController.getCurrentBookIndex())) {
    currentEntryIndex = 0;
  }

  currentBookIndex  = booksDirController.getCurrentBookIndex();
  currentEntryIndex = tocViewer->showPageAndHighlight(currentEntryIndex);
}

auto TocController::leave(bool goingToDeepSleep) -> void {
  if (tocViewer) { tocViewer.reset(); }
}

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  auto TocController::inputEvent(const EventMgr::Event &event) -> void {
    switch (event.kind) {
    case EventMgr::EventKind::SWIPE_RIGHT:
      currentEntryIndex = tocViewer->prevPage();
      break;

    case EventMgr::EventKind::SWIPE_LEFT:
      currentEntryIndex = tocViewer->nextPage();
      break;

    case EventMgr::EventKind::TAP:
      currentEntryIndex = tocViewer->getIndexAt(event.x, event.y);
      if ((currentEntryIndex >= 0) && (currentEntryIndex < epub->toc->getEntryCount())) {
        if (epub->toc->getEntry(currentEntryIndex).pageId.offset >= 0) {
          bookController.setCurrentPageId(epub->toc->getEntry(currentEntryIndex).pageId);
          bookController.becomeOwnerOfBook(std::move(epub));
          appController.setController(AppController::Ctrl::BOOK);
        }
      } else {
        bookController.becomeOwnerOfBook(std::move(epub));
        appController.setController(AppController::Ctrl::BOOK);
      }
      break;

    case EventMgr::EventKind::HOLD:
      break;

    case EventMgr::EventKind::RELEASE:
      tocViewer->clearHighlight();
      break;

    default:
      break;
    }
  }
#else
  auto TocController::inputEvent(const EventMgr::Event &event) -> void {
    switch (event.kind) {
    #if EXTENDED_CASE || BLE_KEYPAD
      case EventMgr::EventKind::PREV:
    #else
      case EventMgr::EventKind::DBL_PREV:
        #endif
        currentEntryIndex = tocViewer->prevColumn();
        break;

        #if EXTENDED_CASE || BLE_KEYPAD
          case EventMgr::EventKind::NEXT:
        #else
          case EventMgr::EventKind::DBL_NEXT:
            #endif
            currentEntryIndex = tocViewer->nextColumn();
            break;

            #if EXTENDED_CASE || BLE_KEYPAD
              case EventMgr::EventKind::DBL_PREV:
            #else
              case EventMgr::EventKind::PREV:
                #endif
                currentEntryIndex = tocViewer->prevItem();
                break;

                #if EXTENDED_CASE || BLE_KEYPAD
                  case EventMgr::EventKind::DBL_NEXT:
                #else
                  case EventMgr::EventKind::NEXT:
                    #endif
                    currentEntryIndex = tocViewer->nextItem();
                    break;

                  case EventMgr::EventKind::SELECT:
                    if ((currentEntryIndex >= 0) && (currentEntryIndex < epub->toc->getEntryCount())) {
                      if (epub->toc->getEntry(currentEntryIndex).pageId.offset >= 0) {
                        bookController.setCurrentPageId(epub->toc->getEntry(currentEntryIndex).pageId);
                        bookController.becomeOwnerOfBook(std::move(epub));
                        appController.setController(AppController::Ctrl::BOOK);
                      }
                    }
                    break;

                  case EventMgr::EventKind::DBL_SELECT:
                    bookController.becomeOwnerOfBook(std::move(epub));
                    appController.setController(AppController::Ctrl::BOOK);
                    break;

                  case EventMgr::EventKind::NONE:
                    break;

                  default:
                    break;
    }
  }
#endif