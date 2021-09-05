// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "viewers/books_dir_viewer.hpp"

class BooksDirController
{
  private:
    static constexpr char const * TAG = "BooksDirController";

    int32_t     book_offset;
    int16_t     current_book_index;
    int16_t     last_read_book_index;
    std::string book_filename;
    bool        book_was_shown;

    PageLocs::PageId book_page_id;
    BooksDirViewer * books_dir_viewer;

  public:
    BooksDirController();
    void setup();
    void input_event(EventMgr::Event event);
    void enter();
    void leave(bool going_to_deep_sleep = false);
    void save_last_book(const PageLocs::PageId & page_id, bool going_to_deep_sleep);
    void show_last_book();
    void new_orientation() { if (books_dir_viewer != nullptr) books_dir_viewer->setup(); }

    inline int16_t get_current_book_index() { return current_book_index; }
};

#if __BOOKS_DIR_CONTROLLER__
  BooksDirController books_dir_controller;
#else
  extern BooksDirController books_dir_controller;
#endif
