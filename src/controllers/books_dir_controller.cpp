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

#if EPUB_INKPLATE6_BUILD
  #include "nvs.h"
#endif

#include <string>

BooksDirController::BooksDirController()
{

}

void
BooksDirController::setup()
{
  char book_filename[256];
  char * filename = nullptr;

  book_index = -1;
  book_page_nbr = -1;
  book_was_shown = false;

  #if EPUB_INKPLATE6_BUILD
    nvs_handle_t nvs_handle;
    esp_err_t    err;
  
    if ((err = nvs_open("EPUB-InkPlate", NVS_READONLY, &nvs_handle)) == ESP_OK) {
      size_t size = 256;
      if ((err = nvs_get_str(nvs_handle, "LAST_BOOK", book_filename, &size)) == ESP_OK) {
        filename = book_filename;
        int8_t was_shown;
        if (((err = nvs_get_i16(nvs_handle, "PAGE_NBR", &book_page_nbr) != ESP_OK)) ||
            ((err = nvs_get_i8(nvs_handle, "WAS_SHOWN", &was_shown) != ESP_OK))) {
          filename = nullptr;
          was_shown = 0;
        }
        else {
          book_was_shown = (bool) was_shown;
        }
      }
      nvs_close(nvs_handle);
    }
    if (err != ESP_OK) {
      LOG_E("Unable to process NVS readout: %d", err);
    }
  #else
    FILE * f = fopen(MAIN_FOLDER "/last_book.txt", "r");
    filename = nullptr;
    if (f != nullptr) {

      if (fgets(book_filename, 256, f)) {
        int16_t size = strlen(book_filename) - 1;
        if (book_filename[size] == '\n') book_filename[size] = 0;

        char buffer[20];
        if (fgets(buffer, 20, f)) {
          book_page_nbr = atoi(buffer);

          if (fgets(buffer, 20, f)) {
            int8_t was_shown = atoi(buffer);
            filename = book_filename;
            book_was_shown = (bool) was_shown;
          }
        }
      }

      fclose(f);
    } 
  #endif

  int16_t db_idx = -1;
  if (!books_dir.read_books_directory(filename, db_idx)) {
    LOG_E("There was issues reading books directory.");
  }
  
  if (db_idx != -1) book_index = books_dir.get_sorted_idx(db_idx);
 
  page_nbr = 0;
  current_index = 0;

  LOG_D("Book to show: idx:%d page:%d was_shown:%d", book_index, book_page_nbr, (int)book_was_shown);
}

void 
BooksDirController::enter()
{
  if (book_was_shown && (book_index != -1)) {
    book_was_shown = false;
    static std::string book_filename;
    static std::string book_title;
    const BooksDir::EBookRecord * book;

    book = books_dir.get_book_data(book_index);
    if (book != nullptr) {
      book_filename = BOOKS_FOLDER "/";
      book_filename += book->filename;
      book_title = book->title;
      if (book_controller.open_book_file(book_title, book_filename, book_index, book_page_nbr)) {
        app_controller.set_controller(AppController::BOOK);
      }
    }
  }
  else {
    if (page_nbr >= books_dir_viewer.page_count()) {
      page_nbr = 0;
      current_index = 0;
    }
    books_dir_viewer.show_page(page_nbr, current_index);
  }
}

void 
BooksDirController::leave(bool going_to_deep_sleep)
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