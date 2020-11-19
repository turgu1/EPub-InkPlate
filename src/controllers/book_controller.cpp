// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_CONTROLLER__ 1
#include "controllers/book_controller.hpp"

#include "controllers/app_controller.hpp"
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
  // As we leave, we keep the information required to return to the book
  // in the NVS space. If this is called just before going to deep sleep, we
  // set the "WAS_SHOWN" boolean to true, such that when the device will
  // be booting, it will display the last book at the last page shown.
  #if EPUB_INKPLATE6_BUILD
    nvs_handle_t nvs_handle;
    esp_err_t err;
  
    if (nvs_open("EPUB-InkPlate", NVS_READWRITE, &nvs_handle)) {
      nvs_set_str(nvs_handle, "LAST_BOOK",  the_book_filename.c_str());
      nvs_set_i16(nvs_handle, "PAGE_NBR",   current_page);
       nvs_set_i8(nvs_handle, "WAS_SHOWN", going_to_deep_sleep ? 1 : 0);
      if ((err = nvs_commit(nvs_handle)) != ESP_OK) {
        LOG_E("NVS Commit error: %d", err);
      }
      nvs_close(nvs_handle);
    }
  #else
    FILE * f = fopen(MAIN_FOLDER "/last_book.txt", "w");
    if (f != nullptr) {
      fprintf(f, "%s\n%d\n%d\n",
        the_book_filename.c_str(),
        current_page,
        going_to_deep_sleep ? 1 : 0
      );
      fclose(f);
    } 
  #endif  
}

bool
BookController::open_book_file(std::string & book_title, std::string & book_filename, int16_t book_idx, int16_t page_nbr)
{
  MsgViewer::show(MsgViewer::BOOK, false, true, "Loading a book",
    "The book \" %s \" is loading. Please wait.", book_title.c_str());

  if (epub.open_file(book_filename)) {
    epub.retrieve_page_locs(book_idx);
    the_book_filename = book_filename.substr(book_filename.find_last_of('/') + 1);
    the_book_idx = book_idx;
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