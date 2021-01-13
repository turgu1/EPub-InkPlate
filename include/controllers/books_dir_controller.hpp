// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"

class BooksDirController
{
  private:
    static constexpr char const * TAG = "BooksDirController";

    int16_t page_nbr;
    int16_t current_index;
    
    int32_t book_offset;
    int16_t book_index;
    std::string book_filename;
    bool    book_was_shown;

    PageLocs::PageId book_page_id;
 

  public:
    BooksDirController();
    void setup();
    void key_event(EventMgr::KeyEvent key);
    void enter();
    void leave(bool going_to_deep_sleep = false);
    void save_last_book(const PageLocs::PageId & page_id, bool going_to_deep_sleep);
    void show_last_book();
};

#if __BOOKS_DIR_CONTROLLER__
  BooksDirController books_dir_controller;
#else
  extern BooksDirController books_dir_controller;
#endif
