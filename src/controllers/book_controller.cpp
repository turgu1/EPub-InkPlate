// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_CONTROLLER__ 1
#include "controllers/book_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "models/epub.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/page.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE6_BUILD
  #include "nvs.h"
#endif

#include <string>

void 
BookController::enter()
{
  book_viewer.show_page(*current_page_id);
}

void 
BookController::leave(bool going_to_deep_sleep)
{
  books_dir_controller.save_last_book(*current_page_id, going_to_deep_sleep);
}

bool
BookController::open_book_file(
  std::string & book_title, 
  std::string & book_filename, 
  int16_t book_idx, 
  const PageLocs::PageId & page_id)
{
  // msg_viewer.show(MsgViewer::BOOK, false, true, "Loading a book",
  //   "The book \" %s \" is loading. Please wait.", book_title.c_str());

  if (epub.open_file(book_filename)) {
    book_viewer.init();
    current_page_id = page_locs.get_page_id(page_id);
    if (current_page_id != nullptr) {
      book_viewer.show_page(*current_page_id);
      return true;
    }
  }
  return false;
}

void 
BookController::key_event(EventMgr::KeyEvent key)
{
  switch (key) {
    case EventMgr::KEY_PREV:
      current_page_id = page_locs.get_prev_page_id(*current_page_id);
      if (current_page_id != nullptr) {
        book_viewer.show_page(*current_page_id);
      }
      break;
    case EventMgr::KEY_DBL_PREV:
      current_page_id = page_locs.get_prev_page_id(*current_page_id, 10);
      if (current_page_id != nullptr) {
        book_viewer.show_page(*current_page_id);
      }
      break;
    case EventMgr::KEY_NEXT:
      current_page_id = page_locs.get_next_page_id(*current_page_id);
      if (current_page_id != nullptr) {
        book_viewer.show_page(*current_page_id);
      }
      break;
    case EventMgr::KEY_DBL_NEXT:
      current_page_id = page_locs.get_next_page_id(*current_page_id, 10);
      if (current_page_id != nullptr) {
        book_viewer.show_page(*current_page_id);
      }
      break;
    
    case EventMgr::KEY_SELECT:
    case EventMgr::KEY_DBL_SELECT:
      app_controller.set_controller(AppController::PARAM);
      break;
    case EventMgr::KEY_NONE:
      break;
  }
}