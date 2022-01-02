// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

class TocViewer
{
  private:
    static constexpr char const * TAG = "TocView";

    static const int16_t TITLE_FONT            =   5;
    static const int16_t ENTRY_FONT            =   5;
    static const int16_t ENTRY_FONT_SIZE       =  11;
    static const int16_t TITLE_FONT_SIZE       =  14;
    static const int16_t MAX_TITLE_SIZE        =  90;
    static const int16_t TITLE_YPOS            =  20;

    #if INKPLATE_6PLUS
      static const int16_t ENTRY_HEIGHT        =  40;
      static const int16_t FIRST_ENTRY_YPOS    = 100;
    #else
      static const int16_t ENTRY_HEIGHT        =  26;
      static const int16_t FIRST_ENTRY_YPOS    =  80;
    #endif

    int16_t current_entry_idx;
    int16_t current_screen_idx;
    int16_t current_page_nbr;
    int16_t entries_per_page;
    int16_t page_count;

    void show_page(int16_t page_nbr, int16_t screen_item_idx);
    void highlight(int16_t item_idx);

  public:

    TocViewer() : current_entry_idx(-1), current_page_nbr(-1) {}
    
    void setup();
    
    int16_t show_page_and_highlight(int16_t entry_idx);
    void            highlight_entry(int16_t entry_idx);
    void            clear_highlight() { }

    int16_t   next_page();
    int16_t   prev_page();
    int16_t   next_item();
    int16_t   prev_item();
    int16_t next_column();
    int16_t prev_column();

    int16_t get_index_at(uint16_t x, uint16_t y) {
      int16_t idx = (y - FIRST_ENTRY_YPOS) / ENTRY_HEIGHT;
      return (idx >= entries_per_page) ? -1 : (current_page_nbr * entries_per_page) + idx;
    }
};

#if __TOC_VIEWER__
  TocViewer toc_viewer;
#else
  extern TocViewer toc_viewer;
#endif
