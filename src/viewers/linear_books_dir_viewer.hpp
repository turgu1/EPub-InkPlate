// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/books_dir.hpp"
#include "viewers/books_dir_viewer.hpp"
#include "viewers/page.hpp"

using LinearBooksDirViewerPtr = himem_unique_ptr<class LinearBooksDirViewer>;
class LinearBooksDirViewer : public BooksDirViewer {
private:
  static constexpr char const *TAG = "LinearBooksDirView";

  static const int16_t TITLE_FONT            = 1;
  static const int16_t AUTHOR_FONT           = 2;
  static const int16_t TITLE_FONT_SIZE       = 11;
  static const int16_t AUTHOR_FONT_SIZE      = 9;
  static const int16_t FIRST_ENTRY_YPOS      = 5;
  static const int16_t SPACE_BETWEEN_ENTRIES = 6;
  static const int16_t MAX_TITLE_SIZE        = 85;

  int16_t current_item_idx{-1};
  int16_t current_book_idx{-1};
  int16_t current_page_nbr{-1};
  int16_t books_per_page{-1};
  int16_t page_count{-1};

  void show_page(int16_t page_nbr, int16_t hightlight_item_idx);
  void highlight(int16_t item_idx);

  PagePtr page{Page::Make()};

  LinearBooksDirViewer() = default;

public:
  ~LinearBooksDirViewer() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make() { return make_unique_himem<LinearBooksDirViewer>(); }

  void setup();

  int16_t show_page_and_highlight(int16_t book_idx);
  void highlight_book(int16_t book_idx);
  void clear_highlight() {}

  int16_t next_page();
  int16_t prev_page();
  int16_t next_item();
  int16_t prev_item();
  int16_t next_column();
  int16_t prev_column();

  int16_t get_index_at(uint16_t x, uint16_t y) {
    int16_t idx = (y - FIRST_ENTRY_YPOS) / (BooksDir::cover_dim.height + SPACE_BETWEEN_ENTRIES);
    return (idx >= books_per_page) ? -1 : (current_page_nbr * books_per_page) + idx;
  }
};
