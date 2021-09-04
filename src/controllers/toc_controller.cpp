// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __TOC_CONTROLLER__ 1
#include "controllers/toc_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "controllers/book_controller.hpp"
#include "models/books_dir.hpp"
#include "models/config.hpp"
#include "models/toc.hpp"

#include "logging.hpp"

#if EPUB_INKPLATE_BUILD
  #include "nvs.h"
#endif

#include <string>

void 
TocController::enter()
{
  toc_viewer.setup();
  
  if ((current_entry_index == -1) || 
      (current_book_index != books_dir_controller.get_current_book_index())) {
    current_entry_index = 0;
  }

  current_book_index  = books_dir_controller.get_current_book_index();
  current_entry_index = toc_viewer.show_page_and_highlight(current_entry_index);
}

#if INKPLATE_6PLUS || TOUCH_TRIAL
  void 
  TocController::input_event(EventMgr::Event event)
  {
    static std::string book_fname;
    static std::string book_title;
    uint16_t x, y;

    const BooksDir::EBookRecord * book;

    switch (event) {
      case EventMgr::Event::SWIPE_RIGHT:
        current_entry_index = toc_viewer.prev_page();   
        break;

      case EventMgr::Event::SWIPE_LEFT:
        current_entry_index = toc_viewer.next_page();   
        break;

      case EventMgr::Event::TAP:
        event_mgr.get_start_location(x, y);

        current_entry_index = toc_viewer.get_index_at(x, y);
        if ((current_entry_index >= 0) && (current_entry_index < toc.get_entry_count())) {
          book_controller.set_current_page_id(toc.get_entry(current_entry_index).page_id);
          app_controller.set_controller(AppController::Ctrl::BOOK);
        }
        else {
          app_controller.set_controller(AppController::Ctrl::BOOK);
        }
        break;

      case EventMgr::Event::HOLD:
        break;

      case EventMgr::Event::RELEASE:
        toc_viewer.clear_highlight();
        break;

      default:
        break;
    }
  }
#else
  void 
  TocController::input_event(EventMgr::Event event)
  {
    switch (event) {
      #if EXTENDED_CASE
        case EventMgr::Event::PREV:
      #else
        case EventMgr::Event::DBL_PREV:
      #endif
        current_entry_index = toc_viewer.prev_column();   
        break;

      #if EXTENDED_CASE
        case EventMgr::Event::NEXT:
      #else
        case EventMgr::Event::DBL_NEXT:
      #endif
        current_entry_index = toc_viewer.next_column();
        break;

      #if EXTENDED_CASE
        case EventMgr::Event::DBL_PREV:
      #else
        case EventMgr::Event::PREV:
      #endif
        current_entry_index = toc_viewer.prev_item();
        break;

      #if EXTENDED_CASE
        case EventMgr::Event::DBL_NEXT:
      #else
        case EventMgr::Event::NEXT:
      #endif
        current_entry_index = toc_viewer.next_item();
        break;

      case EventMgr::Event::SELECT:
        if (current_entry_index < toc.get_entry_count()) {
          book_controller.set_current_page_id(toc.get_entry(current_entry_index).page_id);
          app_controller.set_controller(AppController::Ctrl::BOOK);
        }
        break;

      case EventMgr::Event::DBL_SELECT:
        app_controller.set_controller(AppController::Ctrl::BOOK);
        break;
        
      case EventMgr::Event::NONE:
        break;
    }
  }
#endif