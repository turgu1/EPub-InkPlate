// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __BOOK_VIEWER_HPP__
#define __BOOK_VIEWER_HPP__

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

using namespace pugi;

class BookViewer
{
  private:
    static constexpr char const * TAG = "BookViewer";

    std::mutex mutex;
    #if EPUB_LINUX_BUILD
    #else
      //static SemaphoreHandle_t mutex;
      //static StaticSemaphore_t mutex_buffer;
      //inline static void enter() { xSemaphoreTake(mutex, portMAX_DELAY); }
      //inline static void leave() { xSemaphoreGive(mutex); }
    #endif

    int32_t           current_offset;          ///< Where we are in current item
    int32_t           start_of_page_offset;
    int32_t           end_of_page_offset;
    int16_t           page_bottom;
    bool              show_images;
    PageLocs::PageId  current_page_id;

    bool start_of_paragraph;  ///< Required to manage paragraph indentation at beginning of new page.
    bool indent_paragraph;

    bool                get_image(std::string & filename, Page::Image & image);

    bool       build_page_recurse(pugi::xml_node node, Page::Format fmt);
    void            build_page_at(const PageLocs::PageId & page_id);

  public:

    BookViewer() { }
   ~BookViewer() { }

    void init() { current_page_id = PageLocs::PageId(-1, -1); }
    inline std::mutex & get_mutex() { return mutex; }

    /**
     * @brief Show a page on the display.
     * 
     * @param page_nbr The page number to show (First ebook page = 0, cover = -1)
     */
    void show_page(const PageLocs::PageId & page_id);

    void line_added_at(int16_t ypos) {
      LOG_D("Line added: %d %d", current_offset, ypos);
    }
};

#if __BOOK_VIEWER__
  BookViewer book_viewer;
#else
  extern BookViewer book_viewer;
#endif

#endif