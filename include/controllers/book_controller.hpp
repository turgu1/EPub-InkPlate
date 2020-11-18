// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __BOOK_CONTROLLER_HPP__
#define __BOOK_CONTROLLER_HPP__

#include "global.hpp"
#include "controllers/event_mgr.hpp"

#include <string>

class BookController
{
  public:
    BookController() : current_page(0) {}
    
    void key_event(EventMgr::KeyEvent key);
    void enter();
    void leave();
    bool open_book_file(std::string & book_title, std::string & book_filename, int16_t book_idx);
    void put_str(const char * str, int xpos, int ypos);

  private:
    static constexpr char const * TAG = "BookController";

    int16_t current_page;
};

#if __BOOK_CONTROLLER__
  BookController book_controller;
#else
  extern BookController book_controller;
#endif

#endif