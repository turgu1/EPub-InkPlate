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

#if EPUB_INKPLATE_BUILD
  #include "models/nvs_mgr.hpp"
#endif

void
BooksDirController::setup()
{
  // Retrieve the information related to the last book read by the user. 
  // This is stored in the NVS on the ESP32, or in a flat file on Linux.
  // If the user was reading a book at the last entry to deep sleep, it will be
  // shown on screen instead of the books directory list.

  current_book_index         = -1;
  last_read_book_index       = -1;
  book_page_id.itemref_index = -1;
  book_page_id.offset        = -1;
  book_was_shown             = false;

  
  #if EPUB_INKPLATE_BUILD

    int16_t dummy;
    if (!books_dir.read_books_directory(nullptr, dummy)) {
      LOG_E("There was issues reading books directory.");
    }
    else {

      NVSMgr::NVSData nvs_data;
      uint32_t        id;

      if (nvs_mgr.get_last(id, nvs_data)) {
        book_page_id.itemref_index = nvs_data.itemref_index;
        book_page_id.offset        = nvs_data.offset;
        book_was_shown             = nvs_data.was_shown;

        int16_t idx;
        if ((idx = books_dir.get_sorted_idx_from_id(id)) != -1) {

          last_read_book_index = current_book_index = idx;
          
          //LOG_D("Last book filename: %s",  book_fname);
          LOG_D("Last book ref index: %d", book_page_id.itemref_index);
          LOG_D("Last book offset: %d",    book_page_id.offset);
          LOG_D("Show it now: %s",         book_was_shown ? "yes" : "no");
        }
      }
    }

  #else

    char * book_fname          = new char[256];
    char * filename            = nullptr;

    book_fname[0]              =  0;

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
      current_book_index   = last_read_book_index;
      book_filename        = book_fname;
    }

    LOG_D("Book to show: idx:%d page:(%d, %d) was_shown:%s", 
          last_read_book_index, book_page_id.itemref_index, book_page_id.offset, book_was_shown ? "yes" : "no");

    delete [] book_fname;
  #endif
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

    uint32_t book_id;

    if ((current_book_index != -1) && books_dir.get_book_id(current_book_index, book_id)) {

      NVSMgr::NVSData nvs_data = {
        .offset        = page_id.offset,
        .itemref_index = page_id.itemref_index,
        .was_shown     = (uint8_t) (going_to_deep_sleep ? 1 : 0),
        .filler1       = 0
      };

      if (!nvs_mgr.save_location(book_id, nvs_data)) {
        LOG_E("Unable to save current ebook location");
      }
      last_read_book_index = 
      current_book_index   = books_dir.get_sorted_idx_from_id(book_id);
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

  LOG_D("===> show_last_book()...");
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

  LOG_D("===> enter()...");
  config.get(Config::Ident::DIR_VIEW, &viewer_id);
  books_dir_viewer = (viewer_id == LINEAR_VIEWER) ? (BooksDirViewer *) &linear_books_dir_viewer : 
                                        (BooksDirViewer *) &matrix_books_dir_viewer;

  books_dir_viewer->setup();
  screen.force_full_update();
  
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
  BooksDirController::input_event(const EventMgr::Event & event)
  {
    static std::string book_fname;
    static std::string book_title;

    const BooksDir::EBookRecord * book;

    switch (event.kind) {
      case EventMgr::EventKind::SWIPE_RIGHT:
        current_book_index = books_dir_viewer->prev_page();   
        break;

      case EventMgr::EventKind::SWIPE_LEFT:
        current_book_index = books_dir_viewer->next_page();   
        break;

      case EventMgr::EventKind::TAP:
        if ((viewer_id == MATRIX_VIEWER) || (event.x < (Screen::WIDTH / 3))) {
          current_book_index = books_dir_viewer->get_index_at(event.x, event.y);
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
          else {
            app_controller.set_controller(AppController::Ctrl::OPTION);
          }
        }
        else {
          app_controller.set_controller(AppController::Ctrl::OPTION);
        }
        break;

      case EventMgr::EventKind::HOLD:
        current_book_index = books_dir_viewer->get_index_at(event.x, event.y);
        if ((current_book_index >= 0) && (current_book_index < books_dir.get_book_count())) {
          books_dir_viewer->highlight_book(current_book_index);
          LOG_I("Book Index: %d", current_book_index);
        }
        break;

      case EventMgr::EventKind::RELEASE:
        #if INKPLATE_6PLUS
          ESP::delay(1000);
        #endif
        
        books_dir_viewer->clear_highlight();
        break;

      default:
        break;
    }
  }
#else
  void 
  BooksDirController::input_event(const EventMgr::Event & event)
  {
    static std::string book_fname;
    static std::string book_title;

    const BooksDir::EBookRecord * book;

    switch (event.kind) {
      #if EXTENDED_CASE
        case EventMgr::EventKind::PREV:
      #else
        case EventMgr::EventKind::DBL_PREV:
      #endif
        current_book_index = books_dir_viewer->prev_column();   
        break;

      #if EXTENDED_CASE
        case EventMgr::EventKind::NEXT:
      #else
        case EventMgr::EventKind::DBL_NEXT:
      #endif
        current_book_index = books_dir_viewer->next_column();
        break;

      #if EXTENDED_CASE
        case EventMgr::EventKind::DBL_PREV:
      #else
        case EventMgr::EventKind::PREV:
      #endif
        current_book_index = books_dir_viewer->prev_item();
        break;

      #if EXTENDED_CASE
        case EventMgr::EventKind::DBL_NEXT:
      #else
        case EventMgr::EventKind::NEXT:
      #endif
        current_book_index = books_dir_viewer->next_item();
        break;

      case EventMgr::EventKind::SELECT:
        if (current_book_index < books_dir.get_book_count()) {
          book = books_dir.get_book_data(current_book_index);
          if (book != nullptr) {
            last_read_book_index = current_book_index;
            book_fname    = BOOKS_FOLDER "/";
            book_fname   += book->filename;
            book_title    = book->title;
            book_filename = book->filename;
            
            PageLocs::PageId page_id = { 0, 0 };

            #if EPUB_INKPLATE_BUILD
              NVSMgr::NVSData nvs_data;
              if (nvs_mgr.get_location(book->id, nvs_data)) {
                page_id.itemref_index = nvs_data.itemref_index;
                page_id.offset        = nvs_data.offset;
              }
            #endif

            if (book_controller.open_book_file(book_title, book_fname, page_id)) {
              app_controller.set_controller(AppController::Ctrl::BOOK);
            }
          }
        }
        break;

      case EventMgr::EventKind::DBL_SELECT:
        app_controller.set_controller(AppController::Ctrl::OPTION);
        break;
        
      case EventMgr::EventKind::NONE:
        break;
    }
  }
#endif