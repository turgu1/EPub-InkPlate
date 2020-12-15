// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOKS_DIR_CONTROLLER__ 1
#include "controllers/books_dir_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/book_controller.hpp"
#include "models/books_dir.hpp"
#include "viewers/books_dir_viewer.hpp"
#include "viewers/book_viewer.hpp"
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
  // Retrieve the information related to the last book read by the user. 
  // This is stored in the NVS on the ESP32, or in a flat file on Linux.
  // If the user was reading a book at the last entry to deep sleep, it will be
  // shown on screen instead of the books directory list.

  char book_fname[256];
  
  char * filename = nullptr;
  book_fname[0]   = 0;
  book_index      = -1;
  book_page_id.itemref_index = -1;
  book_page_id.offset        = -1;

  book_was_shown  = false;

  #if EPUB_INKPLATE6_BUILD
    nvs_handle_t nvs_handle;
    esp_err_t    err;
  
    if ((err = nvs_open("EPUB-InkPlate", NVS_READONLY, &nvs_handle)) == ESP_OK) {
      size_t size = 256;
      if ((err = nvs_get_str(nvs_handle, "LAST_BOOK", book_fname, &size)) == ESP_OK) {
        filename = book_fname;
        int8_t was_shown;
        if (((err = nvs_get_i16(nvs_handle, "REF_IDX",   &book_page_id.itemref_index) != ESP_OK)) ||
            ((err = nvs_get_i32(nvs_handle, "OFFSET",    &book_page_id.offset       ) != ESP_OK)) ||
            ((err =  nvs_get_i8(nvs_handle, "WAS_SHOWN", &was_shown                 ) != ESP_OK))) {
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
      LOG_E("Unable to process NVS readout: %s", esp_err_to_name(err));
    }

    LOG_D("Last book filename: %s",  book_fname);
    LOG_D("Last book ref index: %d", book_page_id.itemref_index);
    LOG_D("Last book offset: %d",    book_page_id.offset);
    LOG_D("Show it now: %s",         book_was_shown ? "yes" : "no");
  #else
    FILE * f = fopen(MAIN_FOLDER "/last_book.txt", "r");
    filename = nullptr;
    if (f != nullptr) {

      if (fgets(book_fname, 256, f)) {
        int16_t size = strlen(book_fname) - 1;
        if (book_fname[size] == '\n') book_fname[size] = 0;

        char buffer[20];
        if (fgets(buffer, 20, f)) {
          book_page_id.itemref_index = atoi(buffer);

          if (fgets(buffer, 20, f)) {
            book_page_id.offset = atoi(buffer);

            if (fgets(buffer, 20, f)) {
              int8_t was_shown = atoi(buffer);
              filename       = book_fname;
              book_was_shown = (bool) was_shown;
            }
          }
        }
      }

      fclose(f);
    } 
  #endif

  int16_t db_idx = -1;
  // Read the directory, returning the book index (db_idx).
  if (!books_dir.read_books_directory(filename, db_idx)) {
    LOG_E("There was issues reading books directory.");
  }
  
  // The retrieved db_idx is the index in the database of the last book
  // read by the user. We need the
  // index in the sorted list of books as this is what the 
  // BookController expect.

  if (db_idx != -1) {
    book_index    = books_dir.get_sorted_idx(db_idx);
    book_filename = book_fname;
  }
 
  page_nbr      = 0;
  current_index = 0;

  LOG_D("Book to show: idx:%d page:(%d, %d) was_shown:%s", 
        book_index, book_page_id.itemref_index, book_page_id.offset, book_was_shown ? "yes" : "no");
}

void
BooksDirController::save_last_book(const PageLocs::PageId & page_id, bool going_to_deep_sleep)
{
  // As we leave, we keep the information required to return to the book
  // in the NVS space. If this is called just before going to deep sleep, we
  // set the "WAS_SHOWN" boolean to true, such that when the device will
  // be booting, it will display the last book at the last page shown.

  book_page_id = page_id;

  #if EPUB_INKPLATE6_BUILD
    nvs_handle_t nvs_handle;
    esp_err_t err;
  
    if ((err = nvs_open("EPUB-InkPlate", NVS_READWRITE, &nvs_handle)) == ESP_OK) {

      nvs_set_str(nvs_handle, "LAST_BOOK",  book_filename.c_str());
      nvs_set_i16(nvs_handle, "REF_IDX",    page_id.itemref_index);
      nvs_set_i32(nvs_handle, "OFFSET",     page_id.offset);
       nvs_set_i8(nvs_handle, "WAS_SHOWN",  going_to_deep_sleep ? 1 : 0);

      if ((err = nvs_commit(nvs_handle)) != ESP_OK) {
        LOG_E("NVS Commit error: %s", esp_err_to_name(err));
      }
      nvs_close(nvs_handle);
    }
    else {
      LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
    }
  #else
    FILE * f = fopen(MAIN_FOLDER "/last_book.txt", "w");
    if (f != nullptr) {
      fprintf(f, "%s\n%d\n%d\n%d\n",
        book_filename.c_str(),
        page_id.itemref_index,
        page_id.offset,
        going_to_deep_sleep ? 1 : 0
      );
      fclose(f);
    } 
  #endif  
}

void
BooksDirController::show_last_book()
{
  if (book_index == -1) return;

  static std::string            book_fname;
  static std::string            book_title;
  const BooksDir::EBookRecord * book;

  book_was_shown = false;  
  book           = books_dir.get_book_data(book_index);

  if (book != nullptr) {
    book_fname  = BOOKS_FOLDER "/";
    book_fname += book->filename;
    book_title  = book->title;
    if (book_controller.open_book_file(book_title, book_fname, book_index, book_page_id)) {
      app_controller.set_controller(AppController::BOOK);
    }
  }
}

void 
BooksDirController::enter()
{
  if (book_was_shown && (book_index != -1)) {
    show_last_book();
  }
  else {
    if (page_nbr >= books_dir_viewer.page_count()) {
      page_nbr      = 0;
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
  static std::string book_fname;
  static std::string book_title;

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
        if (current_index > max_index) current_index = max_index;
        books_dir_viewer.highlight(current_index);
      }
      break;
    case EventMgr::KEY_SELECT:
      book_index = (page_nbr * BooksDirViewer::BOOKS_PER_PAGE) + current_index;
      if (book_index < books_dir.get_book_count()) {
        book = books_dir.get_book_data(book_index);
        if (book != nullptr) {
          book_fname    = BOOKS_FOLDER "/";
          book_fname   += book->filename;
          book_title    = book->title;
          book_filename = book->filename;
          
          PageLocs::PageId page_id = { 0, 0 };
          
          if (book_controller.open_book_file(book_title, book_fname, book_index, page_id)) {
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