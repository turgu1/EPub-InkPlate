// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "models/books_dir.hpp"
#include "viewers/page.hpp"
#include "viewers/books_dir_viewer.hpp"

class LinearBooksDirViewer : public BooksDirViewer
{
  private:
    static constexpr char const * TAG = "LinearBooksDirView";

    static const int16_t TITLE_FONT            =  5;
    static const int16_t AUTHOR_FONT           =  6;
    static const int16_t PAGENBR_FONT          =  5;
    static const int16_t TITLE_FONT_SIZE       = 11;
    static const int16_t AUTHOR_FONT_SIZE      =  9;
    static const int16_t PAGENBR_FONT_SIZE     =  9;
    static const int16_t FIRST_ENTRY_YPOS      =  5;
    static const int16_t SPACE_BETWEEN_ENTRIES =  6;
    static const int16_t MAX_TITLE_SIZE        = 85;

    int16_t current_item_idx;
    int16_t current_book_idx;
    int16_t current_page_nbr;
    int16_t books_per_page;
    int16_t page_count;

    void       show_page(int16_t page_nbr, int16_t hightlight_item_idx);
    void       highlight(int16_t item_idx);

  public:

    LinearBooksDirViewer() : current_item_idx(-1), current_page_nbr(-1) {}
    
    void setup();
    
    int16_t   show_page_and_highlight(int16_t book_idx);
    void               highlight_book(int16_t book_idx);
    void              clear_highlight() { }

    int16_t   next_page();
    int16_t   prev_page();
    int16_t   next_item();
    int16_t   prev_item();
    int16_t next_column();
    int16_t prev_column();

    int16_t get_index_at(uint16_t x, uint16_t y) {
      int16_t idx = (y - FIRST_ENTRY_YPOS) / (BooksDir::max_cover_height + SPACE_BETWEEN_ENTRIES);
      return (idx >= books_per_page) ? -1 : (current_page_nbr * books_per_page) + idx;
    }
};

#if __LINEAR_BOOKS_DIR_VIEWER__
  LinearBooksDirViewer linear_books_dir_viewer;
#else
  extern LinearBooksDirViewer linear_books_dir_viewer;
#endif
