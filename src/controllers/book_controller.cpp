// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_CONTROLLER__ 1
#include "controllers/book_controller.hpp"

#include "controllers/app_controller.hpp"
#include "models/epub.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/page.hpp"

#include <string>

void 
BookController::enter()
{
  book_viewer.show_page(current_page);
}

void 
BookController::leave()
{

}

bool
BookController::open_book_file(std::string & book_filename, int16_t book_idx)
{
  if (epub.open_file(book_filename)) {
    epub.retrieve_page_locs(book_idx);
    current_page = 0;
    return true;
  }
  return false;
}

void 
BookController::key_event(EventMgr::KeyEvent key)
{
  switch (key) {
    case EventMgr::KEY_LEFT:
      if (current_page > 0) {
        book_viewer.show_page(--current_page);
      }
      break;
    case EventMgr::KEY_UP:
      current_page -= 10;
      if (current_page < 0) current_page = 0;
      book_viewer.show_page(current_page);
      break;
    case EventMgr::KEY_RIGHT:
      current_page += 1;
      if (current_page >= epub.get_page_count()) {
        current_page = epub.get_page_count() - 1;
      }
      book_viewer.show_page(current_page);
      break;
    case EventMgr::KEY_DOWN:
      current_page += 10;
      if (current_page >= epub.get_page_count()) {
        current_page = epub.get_page_count() - 1;
      }
      book_viewer.show_page(current_page);
      break;
    case EventMgr::KEY_SELECT: {
        for (int i = 0; i < epub.get_page_count(); i++) {
          current_page = i;
          book_viewer.show_page(i);
        }
      }
      break;
    case EventMgr::KEY_HOME:
      app_controller.set_controller(AppController::PARAM);
      break;
  }
}