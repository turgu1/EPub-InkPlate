// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __BOOKS_DIR_VIEWER_HPP__
#define __BOOKS_DIR_VIEWER_HPP__

#include "global.hpp"
#include "models/books_dir.hpp"
#include "viewers/page.hpp"

class BooksDirViewer
{
  private:
    static constexpr char const * TAG = "BooksDirView";

    static const int16_t FIRST_ENTRY_YPOS  = 20;
    static const int16_t LEFT_POS          = 30;
    static const int16_t TITLE_FONT_SIZE   = 11;
    static const int16_t AUTHOR_FONT_SIZE  =  9;
    static const int16_t PAGENBR_FONT_SIZE =  9;

    int16_t current_item_idx;
    int16_t current_page_nbr;

  public:
    static const int16_t BOOKS_PER_PAGE = 8;

    BooksDirViewer() : current_item_idx(-1), current_page_nbr(-1) {}
    
    int16_t page_count() {
      return (books_dir.get_book_count() + BOOKS_PER_PAGE - 1) / BOOKS_PER_PAGE;
    }
    void show_page(int16_t page_nbr, int16_t hightlight_item_idx);
    void highlight(int16_t item_idx);
};

#if __BOOKS_DIR_VIEWER__
  BooksDirViewer books_dir_viewer;
#else
  extern BooksDirViewer books_dir_viewer;
#endif

#endif