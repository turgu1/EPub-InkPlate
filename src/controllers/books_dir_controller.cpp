// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOKS_DIR_CONTROLLER__ 1
#include "controllers/books_dir_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/book_controller.hpp"
#include "models/books_dir.hpp"
#include "models/config.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/linear_books_dir_viewer.hpp"
#include "viewers/matrix_books_dir_viewer.hpp"

#include "logging.hpp"

#if EPUB_INKPLATE_BUILD
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
  
  char * filename            = nullptr;

  book_fname[0]              =  0;
  current_book_index         = -1;
  last_read_book_index       = -1;
  book_page_id.itemref_index = -1;
  book_page_id.offset        = -1;
  book_was_shown             = false;

  #if EPUB_INKPLATE_BUILD
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
    last_read_book_index = books_dir.get_sorted_idx(db_idx);
    book_filename        = book_fname;
  }

  LOG_D("Book to show: idx:%d page:(%d, %d) was_shown:%s", 
        last_read_book_index, book_page_id.itemref_index, book_page_id.offset, book_was_shown ? "yes" : "no");
}

void
BooksDirController::save_last_book(const PageLocs::PageId & page_id, bool going_to_deep_sleep)
{
  // As we leave, we keep the information required to return to the book
  // in the NVS space. If this is called just before going to deep sleep, we
  // set the "WAS_SHOWN" boolean to true, such that when the device will
  // be booting, it will display the last book at the last page shown.

  book_page_id = page_id;

  #if EPUB_INKPLATE_BUILD
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
  if (last_read_book_index == -1) return;

  static std::string            book_fname;
  static std::string            book_title;
  const BooksDir::EBookRecord * book;

  book_was_shown = false;  
  book           = books_dir.get_book_data(last_read_book_index);

  if (book != nullptr) {
    book_fname  = BOOKS_FOLDER "/";
    book_fname += book->filename;
    book_title  = book->title;
    if (book_controller.open_book_file(book_title, book_fname, book_page_id)) {
      app_controller.set_controller(AppController::Ctrl::BOOK);
    }
  }
}

void 
BooksDirController::enter()
{
  int8_t viewer;

  config.get(Config::Ident::DIR_VIEW, &viewer);
  books_dir_viewer = (viewer == 0) ? (BooksDirViewer *) &linear_books_dir_viewer : 
                                     (BooksDirViewer *) &matrix_books_dir_viewer;

  books_dir_viewer->setup();
  
  if (book_was_shown && (last_read_book_index != -1)) {
    show_last_book();
  }
  else {
    if (current_book_index == -1) current_book_index = 0;
    current_book_index = books_dir_viewer->show_page_and_highlight(current_book_index);
  }
}

void 
BooksDirController::leave(bool going_to_deep_sleep)
{

}

#if INKPLATE_6PLUS || TOUCH_TRIAL
  void 
  BooksDirController::input_event(EventMgr::Event event)
  {
    static std::string book_fname;
    static std::string book_title;
    uint16_t x, y;

    const BooksDir::EBookRecord * book;

    switch (event) {
      case EventMgr::Event::SWIPE_RIGHT:
        current_book_index = books_dir_viewer->prev_page();   
        break;

      case EventMgr::Event::SWIPE_LEFT:
        current_book_index = books_dir_viewer->next_page();   
        break;

      case EventMgr::Event::TAP:
        event_mgr.get_start_location(x, y);

        current_book_index = books_dir_viewer->get_index_at(x, y);
        if ((current_book_index >= 0) && (current_book_index < books_dir.get_book_count())) {
          book = books_dir.get_book_data(current_book_index);
          if (book != nullptr) {
            last_read_book_index = current_book_index;
            book_fname    = BOOKS_FOLDER "/";
            book_fname   += book->filename;
            book_title    = book->title;
            book_filename = book->filename;
            
            PageLocs::PageId page_id = { 0, 0 };
            
            if (book_controller.open_book_file(book_title, book_fname, page_id)) {
              app_controller.set_controller(AppController::Ctrl::BOOK);
            }
          }
        }
        else {open_book_file
          app_controller.set_controller(AppController::Ctrl::OPTION);
        }
        break;

      case EventMgr::Event::HOLD:
        event_mgr.get_start_location(x, y);

        current_book_index = books_dir_viewer->get_index_at(x, y);
        if ((current_book_index >= 0) && (current_book_index < books_dir.get_book_count())) {
          books_dir_viewer->highlight_book(current_book_index);
        }
        break;

      case EventMgr::Event::RELEASE:
        books_dir_viewer->clear_highlight();
        break;

      default:
        break;
    }
  }
#else
  void 
  BooksDirController::input_event(EventMgr::Event event)
  {
    static std::string book_fname;
    static std::string book_title;

    const BooksDir::EBookRecord * book;

    switch (event) {
      #if EXTENDED_CASE
        case EventMgr::Event::PREV:
      #else
        case EventMgr::Event::DBL_PREV:
      #endif
        current_book_index = books_dir_viewer->prev_column();   
        break;

      #if EXTENDED_CASE
        case EventMgr::Event::NEXT:
      #else
        case EventMgr::Event::DBL_NEXT:
      #endif
        current_book_index = books_dir_viewer->next_column();
        break;

      #if EXTENDED_CASE
        case EventMgr::Event::DBL_PREV:
      #else
        case EventMgr::Event::PREV:
      #endif
        current_book_index = books_dir_viewer->prev_item();
        break;

      #if EXTENDED_CASE
        case EventMgr::Event::DBL_NEXT:
      #else
        case EventMgr::Event::NEXT:
      #endif
        current_book_index = books_dir_viewer->next_item();
        break;

      case EventMgr::Event::SELECT:
        if (current_book_index < books_dir.get_book_count()) {
          book = books_dir.get_book_data(current_book_index);
          if (book != nullptr) {
            last_read_book_index = current_book_index;
            book_fname    = BOOKS_FOLDER "/";
            book_fname   += book->filename;
            book_title    = book->title;
            book_filename = book->filename;
            
            PageLocs::PageId page_id = { 0, 0 };
            
            if (book_controller.open_book_file(book_title, book_fname, page_id)) {
              app_controller.set_controller(AppController::Ctrl::BOOK);
            }
          }
        }
        break;

      case EventMgr::Event::DBL_SELECT:
        app_controller.set_controller(AppController::Ctrl::OPTION);
        break;
        
      case EventMgr::Event::NONE:
        break;
    }
  }
#endif