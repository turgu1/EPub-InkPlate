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

    static const int16_t FIRST_ENTRY_YPOS  = 10;
    static const int16_t LEFT_POS          = 30;
    static const int16_t TITLE_FONT_SIZE   = 11;
    static const int16_t AUTHOR_FONT_SIZE  =  9;
    static const int16_t PAGENBR_FONT_SIZE =  9;

    int16_t current_item_idx;
    int16_t current_page_nbr;
    int16_t books_per_page;

  public:
    BooksDirViewer() : current_item_idx(-1), current_page_nbr(-1) {}
    
    void setup();
    
    int16_t page_count() {
      return (books_dir.get_book_count() + books_per_page - 1) / books_per_page;
    }
    void show_page(int16_t page_nbr, int16_t hightlight_item_idx);
    void highlight(int16_t item_idx);

    inline int16_t get_books_per_page() { return books_per_page; }
};

#if __BOOKS_DIR_VIEWER__
  BooksDirViewer books_dir_viewer;
#else
  extern BooksDirViewer books_dir_viewer;
#endif

#endif