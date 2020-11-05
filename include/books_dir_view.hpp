#ifndef __BOOKS_DIR_VIEW_HPP__
#define __BOOKS_DIR_VIEW_HPP__

#include "global.hpp"
#include "books_dir.hpp"
#include "page.hpp"

class BooksDirView
{
  private:
    static const int16_t FIRST_ENTRY_YPOS = 20;
    static const int16_t LEFT_POS = 30;
    static const int16_t TITLE_FONT_SIZE = 10;
    static const int16_t AUTHOR_FONT_SIZE = 8;
    static const int16_t PAGENBR_FONT_SIZE = 7;

    int16_t current_item_idx;
    int16_t current_page_nbr;

  public:
    static const int16_t BOOKS_PER_PAGE = 8;

    BooksDirView() : current_item_idx(-1), current_page_nbr(-1) {}
    
    int16_t page_count() {
      return (books_dir.get_book_count() + BOOKS_PER_PAGE - 1) / BOOKS_PER_PAGE;
    }
    void show_page(int16_t page_nbr, int16_t hightlight_item_idx);
    void highlight(int16_t item_idx);
};

#if __BOOKS_DIR_VIEW__
  BooksDirView books_dir_view;
#else
  extern BooksDirView books_dir_view;
#endif

#endif