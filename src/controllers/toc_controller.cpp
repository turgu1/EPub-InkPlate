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

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  void 
  TocController::input_event(const EventMgr::Event & event)
  {
    switch (event.kind) {
      case EventMgr::EventKind::SWIPE_RIGHT:
        current_entry_index = toc_viewer.prev_page();   
        break;

      case EventMgr::EventKind::SWIPE_LEFT:
        current_entry_index = toc_viewer.next_page();   
        break;

      case EventMgr::EventKind::TAP:
        current_entry_index = toc_viewer.get_index_at(event.x, event.y);
        if ((current_entry_index >= 0) && (current_entry_index < toc.get_entry_count())) {
          if(toc.get_entry(current_entry_index).page_id.offset >= 0) {
            book_controller.set_current_page_id(toc.get_entry(current_entry_index).page_id);
            app_controller.set_controller(AppController::Ctrl::BOOK);
          }
        }
        else {
          app_controller.set_controller(AppController::Ctrl::BOOK);
        }
        break;

      case EventMgr::EventKind::HOLD:
        break;

      case EventMgr::EventKind::RELEASE:
        toc_viewer.clear_highlight();
        break;

      default:
        break;
    }
  }
#else
  void 
  TocController::input_event(const EventMgr::Event & event)
  {
    switch (event.kind) {
      #if EXTENDED_CASE
        case EventMgr::EventKind::PREV:
      #else
        case EventMgr::EventKind::DBL_PREV:
      #endif
        current_entry_index = toc_viewer.prev_column();   
        break;

      #if EXTENDED_CASE
        case EventMgr::EventKind::NEXT:
      #else
        case EventMgr::EventKind::DBL_NEXT:
      #endif
        current_entry_index = toc_viewer.next_column();
        break;

      #if EXTENDED_CASE
        case EventMgr::EventKind::DBL_PREV:
      #else
        case EventMgr::EventKind::PREV:
      #endif
        current_entry_index = toc_viewer.prev_item();
        break;

      #if EXTENDED_CASE
        case EventMgr::EventKind::DBL_NEXT:
      #else
        case EventMgr::EventKind::NEXT:
      #endif
        current_entry_index = toc_viewer.next_item();
        break;

      case EventMgr::EventKind::SELECT:
        if ((current_entry_index >= 0) && (current_entry_index < toc.get_entry_count())) {
          if (toc.get_entry(current_entry_index).page_id.offset >= 0) {
            book_controller.set_current_page_id(toc.get_entry(current_entry_index).page_id);
            app_controller.set_controller(AppController::Ctrl::BOOK);
          }
        }
        break;

      case EventMgr::EventKind::DBL_SELECT:
        app_controller.set_controller(AppController::Ctrl::BOOK);
        break;
        
      case EventMgr::EventKind::NONE:
        break;
    }
  }
#endif