// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include <mutex>

#if EPUB_LINUX_BUILD
#else
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/semphr.h"
#endif

#include <vector>
#include <string>
#include <unordered_map>

#include "global.hpp"
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
  private:
    static constexpr char const * TAG = "BookViewer";

    std::mutex mutex;

    // int32_t           current_offset;          ///< Where we are in current item
    // int32_t           start_of_page_offset;
    // int32_t           end_of_page_offset;
    int16_t           page_bottom;
    //bool              show_images;
    PageLocs::PageId  current_page_id;

    // bool start_of_paragraph;  ///< Required to manage paragraph indentation at beginning of new page.

    //bool                get_image(std::string & filename, Page::Image & image);
    //bool       build_page_recurse(pugi::xml_node node, Page::Format fmt, DOM::Node * dom_node);
    void            build_page_at(const PageLocs::PageId & page_id);

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
