// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "models/books_dir.hpp"
#include "viewers/page.hpp"
#include "viewers/books_dir_viewer.hpp"

class MatrixBooksDirViewer : public BooksDirViewer
{
  private:
    static constexpr char const * TAG = "MatrixBooksDirView";

    static const int16_t TITLE_FONT                =  5;
    static const int16_t AUTHOR_FONT               =  6;
    static const int16_t PAGENBR_FONT              =  5;
    static const int16_t TITLE_FONT_SIZE           = 11;
    static const int16_t AUTHOR_FONT_SIZE          =  9;
    static const int16_t PAGENBR_FONT_SIZE         =  9;
    static const int16_t MIN_SPACE_BETWEEN_ENTRIES =  6;
    static const int16_t SPACE_BELOW_INFO          = 10;
    static const int16_t SPACE_ABOVE_PAGENBR       =  5;
    static const int16_t MAX_TITLE_SIZE            = 85;

    int16_t  current_item_idx; // Relative to the beginning of the page
    int16_t  current_book_idx; // Relative to the beginning of the complete boolk list
    int16_t  current_page_nbr;
    int16_t  books_per_page;
    int16_t  column_count;
    int16_t  line_count;
    int16_t  page_count;
    int16_t  first_entry_ypos;
    uint16_t title_font_height;
    uint16_t author_font_height;
    uint16_t pagenbr_font_height;
    uint8_t  horiz_space_between_entries;
    uint8_t  vert_space_between_entries;

    void       show_page(int16_t page_nbr, int16_t hightlight_item_idx);
    void       highlight(int16_t item_idx);

  public:

    MatrixBooksDirViewer() : current_item_idx(-1), current_page_nbr(-1) {}
    
    void setup();
    
    int16_t  show_page_and_highlight(int16_t book_idx);
    void              highlight_book(int16_t book_idx);
    void             clear_highlight();

    int16_t   next_page();
    int16_t   prev_page();
    int16_t   next_item();
    int16_t   prev_item();
    int16_t next_column();
    int16_t prev_column();

    int16_t get_index_at(uint16_t x, uint16_t y) {
      if ((x < 5) || (y < first_entry_ypos)) return -1;
      int16_t line_idx = (y - first_entry_ypos) / (BooksDir::max_cover_height + vert_space_between_entries);
      int16_t column_idx = (x - 5) / (BooksDir::max_cover_width + horiz_space_between_entries);
      if ((line_idx >= line_count) || (column_idx >= column_count)) return -1;
      return (current_page_nbr * books_per_page) + (column_idx * line_count) + line_idx;
    }
};

#if __MATRIX_BOOKS_DIR_VIEWER__
  MatrixBooksDirViewer matrix_books_dir_viewer;
#else
  extern MatrixBooksDirViewer matrix_books_dir_viewer;
#endif
