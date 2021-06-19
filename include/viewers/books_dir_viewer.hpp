// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "models/books_dir.hpp"
#include "viewers/page.hpp"

class BooksDirViewer
{
  public:
    virtual void                        setup() = 0;

    virtual int16_t   show_page_and_highlight(int16_t book_idx) = 0;
    virtual void               highlight_book(int16_t book_idx) = 0;
    virtual void              clear_highlight() = 0;

    virtual int16_t    next_page() = 0;
    virtual int16_t    prev_page() = 0;
    virtual int16_t    next_item() = 0;
    virtual int16_t    prev_item() = 0;
    virtual int16_t  next_column() = 0;
    virtual int16_t  prev_column() = 0;

    virtual int16_t get_index_at(uint16_t x, uint16_t y) = 0;
};
