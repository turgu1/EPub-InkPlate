// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOKS_DIR_CONTROLLER__ 1
#include "controllers/books_dir_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/book_controller.hpp"
#include "models/books_dir.hpp"
#include "viewers/books_dir_viewer.hpp"
#include "logging.hpp"

#include <string>

BooksDirController::BooksDirController()
{

}

void
BooksDirController::setup()
{
  if (!books_dir.read_books_directory()) {
    LOG_E("There was issues reading books directory.");
  }
  page_nbr = 0;
  current_index = 0;
}

void 
BooksDirController::enter()
{
  if (page_nbr >= books_dir_viewer.page_count()) {
    page_nbr = 0;
    current_index = 0;
  }
  books_dir_viewer.show_page(page_nbr, current_index);
}

void 
BooksDirController::leave()
{

}

void 
BooksDirController::key_event(EventMgr::KeyEvent key)
{
  static std::string book_filename;
  static std::string book_title;

  int16_t book_idx;
  const BooksDir::EBookRecord * book;

  switch (key) {
    case EventMgr::KEY_DBL_PREV:
      if (page_nbr > 0) --page_nbr;
      current_index = 0;
      books_dir_viewer.show_page(page_nbr, current_index);   
      break;
    case EventMgr::KEY_DBL_NEXT:
      if ((page_nbr + 1) < books_dir_viewer.page_count()) {
        current_index = 0;
        books_dir_viewer.show_page(++page_nbr, current_index);
      }
      else {
        current_index = (books_dir.get_book_count() - 1) % BooksDirViewer::BOOKS_PER_PAGE;
        books_dir_viewer.highlight(current_index);
      }
      break;
    case EventMgr::KEY_PREV:
      if (current_index == 0) {
        if (page_nbr > 0) {
          current_index = BooksDirViewer::BOOKS_PER_PAGE - 1;
          books_dir_viewer.show_page(--page_nbr, current_index);
        }
      }
      else {
        current_index--;
        books_dir_viewer.highlight(current_index);
      }
      break;
    case EventMgr::KEY_NEXT:
      if ((current_index + 1) >= BooksDirViewer::BOOKS_PER_PAGE) {
        if ((page_nbr + 1) < books_dir_viewer.page_count()) {
          page_nbr++;
          current_index = 0;
        }
        books_dir_viewer.show_page(page_nbr, current_index);
      }
      else {
        int16_t max_index = BooksDirViewer::BOOKS_PER_PAGE;
        if ((page_nbr + 1) == books_dir_viewer.page_count()) {
          max_index = (books_dir.get_book_count() - 1) % BooksDirViewer::BOOKS_PER_PAGE;
        }
        current_index++;
        if (current_index > max_index) current_index = max_index - 1;
        books_dir_viewer.highlight(current_index);
      }
      break;
    case EventMgr::KEY_SELECT:
      book_idx = (page_nbr * BooksDirViewer::BOOKS_PER_PAGE) + current_index;
      if (book_idx < books_dir.get_book_count()) {
        book = books_dir.get_book_data(book_idx);
        if (book != nullptr) {
          book_filename = BOOKS_FOLDER "/";
          book_filename += book->filename;
          book_title = book->title;
          if (book_controller.open_book_file(book_title, book_filename, book_idx)) {
            app_controller.set_controller(AppController::BOOK);
          }
        }
      }
      break;
    case EventMgr::KEY_DBL_SELECT:
      app_controller.set_controller(AppController::OPTION);
      break;
    case EventMgr::KEY_NONE:
      break;
  }
}