#ifndef __BOOKS_DIR_CONTROLLER_HPP__
#define __BOOKS_DIR_CONTROLLER_HPP__

#include "global.hpp"
#include "eventmgr.hpp"

class BooksDirController
{
  private:
    int16_t page_nbr;
    int16_t current_index;
    
  public:
    BooksDirController();
    void setup();
    void key_event(EventMgr::KeyEvent key);
    void enter();
    void leave();
};

#if __BOOKS_DIR_CONTROLLER__
  BooksDirController books_dir_controller;
#else
  extern BooksDirController books_dir_controller;
#endif

#endif