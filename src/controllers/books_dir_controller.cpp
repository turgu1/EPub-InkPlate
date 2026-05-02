// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOKS_DIR_CONTROLLER__ 1
#include "controllers/books_dir_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/book_controller.hpp"
#include "models/books_dir.hpp"
#include "models/config.hpp"
#include "models/nvs_mgr.hpp"
#include "viewers/book_viewer.hpp"

#include <fstream>

#if EPUB_INKPLATE_BUILD
  #include "models/nvs_mgr.hpp"
#endif

auto BooksDirController::setup() -> void {
  // Retrieve the information related to the last book read by the user.
  // This is stored in the NVS on the ESP32, or in a flat file on Linux.
  // If the user was reading a book at the last entry to deep sleep, it will be
  // shown on screen instead of the books directory list.

  currentBookIndex        = -1;
  lastReadBookIndex       = -1;
  bookPageId.itemrefIndex = -1;
  bookPageId.offset       = -1;
  bookWasShown            = false;

  #if EPUB_INKPLATE_BUILD

    int16_t dummy;
    if (!booksDir.readBooksDirectory(nullptr, dummy)) {
      LOG_E("There was issues reading books directory.");
    } else {

      NVSMgr::NVSData nvsData;
      uint32_t id;

      if (nvsMgr.getLast(id, nvsData)) {
        bookPageId.itemrefIndex = nvsData.itemrefIndex;
        bookPageId.offset       = nvsData.offset;
        bookWasShown            = nvsData.wasShown;

        int16_t idx;
        if ((idx = booksDir.getSortedIdxFromId(id)) != -1) {

          lastReadBookIndex = currentBookIndex = idx;

          // LOG_D("Last book filename: %s",  bookFilenameBuffer);
          LOG_D("Last book ref index: %" PRIi16, bookPageId.itemrefIndex);
          LOG_D("Last book offset: %" PRIi32, bookPageId.offset);
          LOG_D("Show it now: %s", bookWasShown ? "yes" : "no");
        }
      }
    }

  #else

    std::string bookFilenameBuffer;
    char *filename = nullptr;

    std::ifstream f(MAIN_FOLDER "/last_book.txt");
    if (f.is_open() && std::getline(f, bookFilenameBuffer)) {
      if (!bookFilenameBuffer.empty() && (bookFilenameBuffer.back() == '\r')) {
        bookFilenameBuffer.pop_back();
      }

      std::string buffer;
      if (std::getline(f, buffer)) {
        bookPageId.itemrefIndex = atoi(buffer.c_str());

        if (std::getline(f, buffer)) {
          bookPageId.offset = atoi(buffer.c_str());

          if (std::getline(f, buffer)) {
            int8_t wasShown = atoi(buffer.c_str());
            filename        = bookFilenameBuffer.data();
            bookWasShown    = (bool)wasShown;
          }
        }
      }
    }

    int16_t dbIdx = -1;
    // Read the directory, returning the book index (dbIdx).
    if (!booksDir.readBooksDirectory(filename, dbIdx)) {
      LOG_E("There was issues reading books directory.");
    }

    // The retrieved dbIdx is the index in the database of the last book
    // read by the user. We need the
    // index in the sorted list of books as this is what the
    // BookController expect.

    if (dbIdx != -1) {
      lastReadBookIndex = booksDir.getSortedIdx(dbIdx);
      currentBookIndex  = lastReadBookIndex;
      bookFilename      = bookFilenameBuffer;
    }

    LOG_D("Book to show: idx:%d page:(%d, %d) wasShown:%s", lastReadBookIndex,
          bookPageId.itemrefIndex, bookPageId.offset, bookWasShown ? "yes" : "no");

  #endif
}

auto BooksDirController::saveLastBook(const PageId &pageId, bool goingToDeepSleep) -> void {
  // As we leave, we keep the information required to return to the book
  // in the NVS space. If this is called just before going to deep sleep, we
  // set the "WAS_SHOWN" boolean to true, such that when the device will
  // be booting, it will display the last book at the last page shown.

  bookPageId = pageId;

  #if EPUB_INKPLATE_BUILD

    uint32_t bookId;

    if ((currentBookIndex != -1) && booksDir.getBookId(currentBookIndex, bookId)) {

      NVSMgr::NVSData nvsData = {.offset       = pageId.offset,
                                 .itemrefIndex = pageId.itemrefIndex,
                                 .wasShown     = static_cast<uint8_t>(goingToDeepSleep ? 1 : 0),
                                 .filler       = 0};

      if (!nvsMgr.saveLocation(bookId, nvsData)) {
        LOG_E("Unable to save current ebook location");
      }
      lastReadBookIndex = currentBookIndex = booksDir.getSortedIdxFromId(bookId);
    }

  #else

    std::ofstream f(MAIN_FOLDER "/last_book.txt");
    if (f.is_open()) {
      f << bookFilename << '\n'
        << pageId.itemrefIndex << '\n'
        << pageId.offset << '\n'
        << (goingToDeepSleep ? 1 : 0) << '\n';
    }
  #endif
}

auto BooksDirController::showLastBook() -> void {

  if (lastReadBookIndex == -1) return;

  LOG_D("===> showLastBook()...");
  static HimemString bookFilenameLocal;
  static HimemString bookTitleLocal;

  bookWasShown = false;
  auto book    = booksDir.getBookData(lastReadBookIndex);

  if (book) {
    bookFilenameLocal = BOOKS_FOLDER "/";
    bookFilenameLocal += book->filename;
    bookTitleLocal = book->title;
    if (bookController.openBook(bookTitleLocal, bookFilenameLocal, bookPageId)) {
      appController.setController(AppController::Ctrl::BOOK);
    }
  }
}

auto BooksDirController::enter() -> void {

  LOG_D("===> enter()...");
  config.get(Config::Ident::DIR_VIEW, &viewerId);
  if (viewerId == LINEAR_VIEWER) {
    booksDirViewer = LinearBooksDirViewer::Make();
  } else {
    booksDirViewer = MatrixBooksDirViewer::Make();
  }

  booksDirViewer->setup();
  screen.forceFullUpdate();

  if (bookWasShown && (lastReadBookIndex != -1)) {
    showLastBook();
  } else {
    if (currentBookIndex == -1) currentBookIndex = 0;
    currentBookIndex = booksDirViewer->showPageAndHighlight(currentBookIndex);
  }
}

auto BooksDirController::leave(bool goingToDeepSleep) -> void {
  LOG_D("===> leave()...");

  saveLastBook(bookPageId, goingToDeepSleep);

  booksDirViewer.reset();
}

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  auto BooksDirController::inputEvent(const EventMgr::Event &event) -> void {
    static HimemString bookFilenameLocal;
    static HimemString bookTitleLocal;

    // auto book = booksDir.getBookData(lastReadBookIndex);

    switch (event.kind) {
    case EventMgr::EventKind::SWIPE_RIGHT:
      LOG_D("[SWIPE_RIGHT]");
      currentBookIndex = booksDirViewer->prevPage();
      break;

    case EventMgr::EventKind::SWIPE_LEFT:
      LOG_D("[SWIPE_LEFT]");
      currentBookIndex = booksDirViewer->nextPage();
      break;

    case EventMgr::EventKind::TAP:
      if ((viewerId == MATRIX_VIEWER) || (event.x < (Screen::getWidth() / 3))) {
        currentBookIndex = booksDirViewer->getIndexAt(event.x, event.y);
        if ((currentBookIndex >= 0) && (currentBookIndex < booksDir.getBookCount())) {
          LOG_D("[TAP] Book Index: %d", currentBookIndex);
          auto book = booksDir.getBookData(currentBookIndex);
          if (book) {
            lastReadBookIndex = currentBookIndex;
            bookFilenameLocal = BOOKS_FOLDER "/";
            bookFilenameLocal += book->filename;
            bookTitleLocal = book->title;
            bookFilename   = book->filename;

            PageId pageId = {0, 0};

            #if EPUB_INKPLATE_BUILD
              NVSMgr::NVSData nvsData;
              if (nvsMgr.getLocation(book->id, nvsData)) {
                pageId = {nvsData.itemrefIndex, nvsData.offset};
              }
            #endif

            if (bookController.openBook(bookTitleLocal, bookFilenameLocal, pageId)) {
              appController.setController(AppController::Ctrl::BOOK);
            }
          }
        } else {
          LOG_D("[OPTIONS MENU]");
          currentBookIndex = -1;
          appController.setController(AppController::Ctrl::OPTION);
        }
      } else {
        LOG_D("[OPTIONS MENU]");
        currentBookIndex = -1;
        appController.setController(AppController::Ctrl::OPTION);
      }
      break;

    case EventMgr::EventKind::HOLD:
      currentBookIndex = booksDirViewer->getIndexAt(event.x, event.y);
      if ((currentBookIndex >= 0) && (currentBookIndex < booksDir.getBookCount())) {
        LOG_D("[HOLD] Book Index: %d", currentBookIndex);
        booksDirViewer->highlightBook(currentBookIndex);
      }
      break;

    case EventMgr::EventKind::RELEASE:
      LOG_D("[RELEASE]");
      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
        ESP::delay(1000);
      #endif

      booksDirViewer->clearHighlight();
      break;

    default:
      break;
    }
  }
#else
  auto BooksDirController::inputEvent(const EventMgr::Event &event) -> void {
    static HimemString bookFilenameLocal;
    static HimemString bookTitleLocal;

    switch (event.kind) {
      #if EXTENDED_CASE
      case EventMgr::EventKind::PREV:
      #else
      case EventMgr::EventKind::DBL_PREV:
      #endif
      currentBookIndex = booksDirViewer->prevColumn();
      break;

      #if EXTENDED_CASE
      case EventMgr::EventKind::NEXT:
      #else
      case EventMgr::EventKind::DBL_NEXT:
      #endif
      currentBookIndex = booksDirViewer->nextColumn();
      break;

      #if EXTENDED_CASE
      case EventMgr::EventKind::DBL_PREV:
      #else
      case EventMgr::EventKind::PREV:
      #endif
      currentBookIndex = booksDirViewer->prevItem();
      break;

      #if EXTENDED_CASE
      case EventMgr::EventKind::DBL_NEXT:
      #else
      case EventMgr::EventKind::NEXT:
      #endif
      currentBookIndex = booksDirViewer->nextItem();
      break;

    case EventMgr::EventKind::SELECT:
      if (currentBookIndex < booksDir.getBookCount()) {
        auto book = booksDir.getBookData(currentBookIndex);
        if (book) {
          lastReadBookIndex = currentBookIndex;
          bookFilenameLocal = BOOKS_FOLDER "/";
          bookFilenameLocal += book->filename;
          bookTitleLocal = book->title;
          bookFilename   = book->filename;

          PageId pageId = {0, 0};

          #if EPUB_INKPLATE_BUILD
            NVSMgr::NVSData nvsData;
            if (nvsMgr.getLocation(book->id, nvsData)) {
              pageId.itemrefIndex = nvsData.itemrefIndex;
              pageId.offset       = nvsData.offset;
            }
          #endif

          if (bookController.openBook(bookTitleLocal, bookFilenameLocal, pageId)) {
            appController.setController(AppController::Ctrl::BOOK);
          }
        }
      }
      break;

    case EventMgr::EventKind::DBL_SELECT:
      appController.setController(AppController::Ctrl::OPTION);
      break;

    case EventMgr::EventKind::NONE:
      break;
    }
  }
#endif