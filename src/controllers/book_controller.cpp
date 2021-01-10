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

#if EPUB_INKPLATE_BUILD
  #include "nvs.h"
#endif

#include <string>

void 
BookController::enter()
{ 
  page_locs.check_for_format_changes(epub.get_item_count(), current_page_id.itemref_index);
  const PageLocs::PageId * id = page_locs.get_page_id(current_page_id);
  if (id != nullptr) {
    current_page_id.itemref_index = id->itemref_index;
    current_page_id.offset        = id->offset;
  }
  else {
    current_page_id.itemref_index = 0;
    current_page_id.offset        = 0;
  }
  book_viewer.show_page(current_page_id);
}

void 
BookController::leave(bool going_to_deep_sleep)
{
  books_dir_controller.save_last_book(current_page_id, going_to_deep_sleep);
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

  bool new_document = book_filename != epub.get_current_filename();

  if (new_document) page_locs.stop_document();

  if (epub.open_file(book_filename)) {
    if (new_document) {
      page_locs.start_new_document(epub.get_item_count(), page_id.itemref_index);
    }
    else {
      page_locs.check_for_format_changes(epub.get_item_count(), page_id.itemref_index);
    }
    book_viewer.init();
    const PageLocs::PageId * id = page_locs.get_page_id(page_id);
    if (id != nullptr) {
      current_page_id.itemref_index = id->itemref_index;
      current_page_id.offset        = id->offset;
      book_viewer.show_page(current_page_id);
      return true;
    }
  }
  return false;
}

void 
BookController::key_event(EventMgr::KeyEvent key)
{
  const PageLocs::PageId * page_id;
  switch (key) {
    case EventMgr::KeyEvent::PREV:
      page_id = page_locs.get_prev_page_id(current_page_id);
      if (page_id != nullptr) {
        current_page_id.itemref_index = page_id->itemref_index;
        current_page_id.offset        = page_id->offset;
        book_viewer.show_page(current_page_id);
      }
      break;

    case EventMgr::KeyEvent::DBL_PREV:
      page_id = page_locs.get_prev_page_id(current_page_id, 10);
      if (page_id != nullptr) {
        current_page_id.itemref_index = page_id->itemref_index;
        current_page_id.offset        = page_id->offset;
        book_viewer.show_page(current_page_id);
      }
      break;

    case EventMgr::KeyEvent::NEXT:
      page_id = page_locs.get_next_page_id(current_page_id);
      if (page_id != nullptr) {
        current_page_id.itemref_index = page_id->itemref_index;
        current_page_id.offset        = page_id->offset;
        book_viewer.show_page(current_page_id);
      }
      break;

    case EventMgr::KeyEvent::DBL_NEXT:
      page_id = page_locs.get_next_page_id(current_page_id, 10);
      if (page_id != nullptr) {
        current_page_id.itemref_index = page_id->itemref_index;
        current_page_id.offset        = page_id->offset;
        book_viewer.show_page(current_page_id);
      }
      break;
    
    case EventMgr::KeyEvent::SELECT:
    case EventMgr::KeyEvent::DBL_SELECT:
      app_controller.set_controller(AppController::Ctrl::PARAM);
      break;
      
    case EventMgr::KeyEvent::NONE:
      break;
  }
}