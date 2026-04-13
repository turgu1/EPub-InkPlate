// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include <mutex>
#include <unordered_map>

#if EPUB_LINUX_BUILD
#else
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
  #include "freertos/task.h"
#endif

#include "models/css.hpp"
#include "models/dom.hpp"
#include "models/epub.hpp"
#include "models/fonts.hpp"
#include "models/page_locs.hpp"
#include "pugixml.hpp"
#include "viewers/page.hpp"

using namespace pugi;

using BookViewerPtr = himem_unique_ptr<class BookViewer>;

class BookViewer {
private:
  static constexpr char const *TAG = "BookViewer";

  BookViewer() = default;

  std::mutex mutex;
  uint16_t page_bottom;
  PageId current_page_id{-1, -1};

  PagePtr page{Page::Make()};

  void build_page_at(const PageId &page_id, EPubPtr &epub);

  struct PageEnd {
    bool operator()(Page::Format &fmt) const { return false; }
  };

public:
  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make() { return make_unique_himem<BookViewer>(); }

  ~BookViewer() = default;

  inline std::mutex &get_mutex() { return mutex; }

  /**
   * @brief Show a page on the display.
   *
   * @param page_nbr The page number to show (First ebook page = 0, cover = -1)
   */
  void show_page(const PageId &page_id, EPubPtr &epub);

  void show_fake_cover(EPubPtr &epub);

  static constexpr int16_t TITLE_FONT      = 2;
  static constexpr int16_t TITLE_FONT_SIZE = 8;
};
