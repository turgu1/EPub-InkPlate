// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include <mutex>
#include <unordered_map>

#if EPUB_LINUX_BUILD
#else
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/semphr.h"
#endif

#include "pugixml.hpp"
#include "viewers/page.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "models/css.hpp"
#include "models/fonts.hpp"
#include "models/dom.hpp"

using namespace pugi;

class BookViewer
{
  public:
    const int16_t TITLE_FONT      = 2;
    const int16_t TITLE_FONT_SIZE = 8;

  private:
    static constexpr char const * TAG = "BookViewer";

    std::mutex        mutex;
    int16_t           page_bottom;
    PageLocs::PageId  current_page_id;

    void build_page_at(const PageLocs::PageId & page_id);

    struct PageEnd {
      bool operator()(Page::Format & fmt) const {
        return false;
      }
    };

  public:

    BookViewer() { }
   ~BookViewer() { }

    void                     init() { current_page_id = PageLocs::PageId(-1, -1); }
    inline std::mutex & get_mutex() { return mutex; }

    /**
     * @brief Show a page on the display.
     * 
     * @param page_nbr The page number to show (First ebook page = 0, cover = -1)
     */
    void show_page(const PageLocs::PageId & page_id);

    void show_fake_cover();
};

#if __BOOK_VIEWER__
  BookViewer book_viewer;
#else
  extern BookViewer book_viewer;
#endif
