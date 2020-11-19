// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __BOOKS_DIR_CONTROLLER_HPP__
#define __BOOKS_DIR_CONTROLLER_HPP__

#include "global.hpp"
#include "controllers/event_mgr.hpp"

class BooksDirController
{
  private:
    static constexpr char const * TAG = "BooksDirController";

    int16_t page_nbr;
    int16_t current_index;
    
    bool    book_was_showed;
    int16_t book_page_nbr;
    int16_t book_index;


  public:
    BooksDirController();
    void setup();
    void key_event(EventMgr::KeyEvent key);
    void enter();
    void leave(bool going_to_deep_sleep = false);
};

#if __BOOKS_DIR_CONTROLLER__
  BooksDirController books_dir_controller;
#else
  extern BooksDirController books_dir_controller;
#endif

#endif