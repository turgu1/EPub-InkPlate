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

  // When entering the book viewer, the BookController::open_book() method was already called,
  // the epub instance is ready and pointing at the required book.

  book_viewer = BookViewer::Make();

  // As the page formatting options may have changed since the last time the book was requested to
  // be shown, we need to check if the page locations are still valid. If not, we need to
  // recalculate them before showing the current page.

  page_locs.check_for_format_changes(epub, current_page_id.itemref_index);

  const PageId *id = page_locs.get_page_id(current_page_id);
  if (id != nullptr) {
    current_page_id.itemref_index = id->itemref_index;
    current_page_id.offset        = id->offset;
  } else {
    current_page_id.itemref_index = 0;
    current_page_id.offset        = 0;
  }
  book_viewer->show_page(current_page_id, epub);
}

void BookController::leave(bool going_to_deep_sleep) {
  LOG_D("===> leave()...");

  books_dir_controller.save_last_book(current_page_id, going_to_deep_sleep);
  book_viewer.reset();
}

bool BookController::open_book(const std::string &book_title, const std::string &book_filename,
                               const PageId &page_id) {
  LOG_D("===> open_book()...");

  MsgViewer::show(MsgViewer::MsgType::BOOK, false, false, "Loading a book",
                  "The book \" %s \" is loading. Please wait.", book_title.c_str());

  bool new_document = !epub || (book_filename != epub->get_current_filename());

  if (new_document) {
    // In case we are already in the middle of processing another book, we need to
    // stop it before starting a new one.
    page_locs.stop_document();

    epub = EPub::Make();

    if (epub->open(book_filename)) {
      page_locs.start_new_document(epub, page_id.itemref_index);
    } else {
      MsgViewer::show(MsgViewer::MsgType::ALERT, true, true, "Error",
                      "An error occurred while opening the book. Please try again.");
      return false;
    }
  }

  // if (epub->open(book_filename)) {
  //   if (new_document) {
  //     page_locs.start_new_document(epub->get_item_count(), page_id.itemref_index);
  //   } else {
  //     page_locs.check_for_format_changes(epub, page_id.itemref_index);
  //   }
  //   const PageId *id = page_locs.get_page_id(page_id);
  //   if (id != nullptr) {
  //     current_page_id.itemref_index = id->itemref_index;
  //     current_page_id.offset        = id->offset;
  //     // book_viewer->show_page(current_page_id);
  //     return true;
  //   }
  // }

  return true;
}

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  void BookController::input_event(const EventMgr::Event &event) {
    const PageId *page_id;
    switch (event.kind) {
    case EventMgr::EventKind::SWIPE_RIGHT:
      if (event.y < (Screen::get_height() - 40)) {
        page_id = page_locs.get_prev_page_id(current_page_id);
        if (page_id != nullptr) {
          current_page_id.itemref_index = page_id->itemref_index;
          current_page_id.offset        = page_id->offset;
          book_viewer->show_page(current_page_id, epub);
        }
      } else {
        page_id = page_locs.get_prev_page_id(current_page_id, 10);
        if (page_id != nullptr) {
          current_page_id.itemref_index = page_id->itemref_index;
          current_page_id.offset        = page_id->offset;
          book_viewer->show_page(current_page_id, epub);
        }
      }
      break;

    case EventMgr::EventKind::SWIPE_LEFT:
      if (event.y < (Screen::get_height() - 40)) {
        page_id = page_locs.get_next_page_id(current_page_id);
        if (page_id != nullptr) {
          current_page_id.itemref_index = page_id->itemref_index;
          current_page_id.offset        = page_id->offset;
          book_viewer->show_page(current_page_id, epub);
        }
      } else {
        page_id = page_locs.get_next_page_id(current_page_id, 10);
        if (page_id != nullptr) {
          current_page_id.itemref_index = page_id->itemref_index;
          current_page_id.offset        = page_id->offset;
          book_viewer->show_page(current_page_id, epub);
        }
      }
      break;

    case EventMgr::EventKind::TAP:
      if (event.y < (Screen::get_height() - 40)) {
        if (event.x < (Screen::get_width() / 3)) {
          page_id = page_locs.get_prev_page_id(current_page_id);
          if (page_id != nullptr) {
            current_page_id.itemref_index = page_id->itemref_index;
            current_page_id.offset        = page_id->offset;
            book_viewer->show_page(current_page_id, epub);
          }
        } else if (event.x > ((Screen::get_width() / 3) * 2)) {
          page_id = page_locs.get_next_page_id(current_page_id);
          if (page_id != nullptr) {
            current_page_id.itemref_index = page_id->itemref_index;
            current_page_id.offset        = page_id->offset;
            book_viewer->show_page(current_page_id, epub);
          }
        } else {
          book_param_controller.set_ownership_of_book(epub);
          app_controller.set_controller(AppController::Ctrl::PARAM);
        }
      } else {
        book_param_controller.set_ownership_of_book(epub);
        app_controller.set_controller(AppController::Ctrl::PARAM);
      }
      break;

    default:
      break;
    }
  }
#else
  void BookController::input_event(const EventMgr::Event &event) {
    const PageId *page_id;
    switch (event.kind) {
      #if EXTENDED_CASE
      case EventMgr::EventKind::DBL_PREV:
      #else
      case EventMgr::EventKind::PREV:
      #endif
      page_id = page_locs.get_prev_page_id(current_page_id);
      if (page_id != nullptr) {
        current_page_id.itemref_index = page_id->itemref_index;
        current_page_id.offset        = page_id->offset;
        book_viewer->show_page(current_page_id, epub);
      }
      break;

      #if EXTENDED_CASE
      case EventMgr::EventKind::PREV:
      #else
      case EventMgr::EventKind::DBL_PREV:
      #endif
      page_id = page_locs.get_prev_page_id(current_page_id, 10);
      if (page_id != nullptr) {
        current_page_id.itemref_index = page_id->itemref_index;
        current_page_id.offset        = page_id->offset;
        book_viewer->show_page(current_page_id, epub);
      }
      break;

      #if EXTENDED_CASE
      case EventMgr::EventKind::DBL_NEXT:
      #else
      case EventMgr::EventKind::NEXT:
      #endif
      page_id = page_locs.get_next_page_id(current_page_id);
      if (page_id != nullptr) {
        current_page_id.itemref_index = page_id->itemref_index;
        current_page_id.offset        = page_id->offset;
        book_viewer->show_page(current_page_id, epub);
      }
      break;

      #if EXTENDED_CASE
      case EventMgr::EventKind::NEXT:
      #else
      case EventMgr::EventKind::DBL_NEXT:
      #endif
      page_id = page_locs.get_next_page_id(current_page_id, 10);
      if (page_id != nullptr) {
        current_page_id.itemref_index = page_id->itemref_index;
        current_page_id.offset        = page_id->offset;
        book_viewer->show_page(current_page_id, epub);
      }
      break;

    case EventMgr::EventKind::SELECT:
    case EventMgr::EventKind::DBL_SELECT:
      book_param_controller.set_ownership_of_book(epub);
      app_controller.set_controller(AppController::Ctrl::PARAM);
      break;

    case EventMgr::EventKind::NONE:
      break;
    }
  }
#endif
