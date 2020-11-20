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
  book_viewer.show_page(current_page);
}

void 
BookController::leave(bool going_to_deep_sleep)
{
  books_dir_controller.save_last_book(current_page, going_to_deep_sleep);
}

bool
BookController::open_book_file(std::string & book_title, std::string & book_filename, int16_t book_idx, int16_t page_nbr)
{
  MsgViewer::show(MsgViewer::BOOK, false, true, "Loading a book",
    "The book \" %s \" is loading. Please wait.", book_title.c_str());

  if (epub.open_file(book_filename)) {
    epub.retrieve_page_locs(book_idx);
    current_page = page_nbr;
    return true;
  }
  return false;
}

void 
BookController::key_event(EventMgr::KeyEvent key)
{
  switch (key) {
    case EventMgr::KEY_PREV:
      if (current_page > 0) {
        book_viewer.show_page(--current_page);
      }
      break;
    case EventMgr::KEY_DBL_PREV:
      current_page -= 10;
      if (current_page < 0) current_page = 0;
      book_viewer.show_page(current_page);
      break;
    case EventMgr::KEY_NEXT:
      current_page += 1;
      if (current_page >= epub.get_page_count()) {
        current_page = epub.get_page_count() - 1;
      }
      book_viewer.show_page(current_page);
      break;
    case EventMgr::KEY_DBL_NEXT:
      current_page += 10;
      if (current_page >= epub.get_page_count()) {
        current_page = epub.get_page_count() - 1;
      }
      book_viewer.show_page(current_page);
      break;
    
    #if DEBUGGING
      case EventMgr::KEY_SELECT: {
          for (int i = 0; i < epub.get_page_count(); i++) {
            current_page = i;
            book_viewer.show_page(i);
          }
        }
        break;
    #else
      case EventMgr::KEY_SELECT:
    #endif

    case EventMgr::KEY_DBL_SELECT:
      app_controller.set_controller(AppController::PARAM);
      break;
    case EventMgr::KEY_NONE:
      break;
  }
}