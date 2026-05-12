// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

// #include <mutex>
#include <unordered_map>

#if EPUB_LINUX_BUILD
#else
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
  #include "freertos/task.h"
#endif

#include "fonts.hpp"
#include "models/css.hpp"
#include "models/dom.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "pugixml.hpp"
#include "viewers/page.hpp"

using namespace pugi;

using BookViewerPtr = HimemUniquePtr<class BookViewer>;

class BookViewer {
public:
  static constexpr char const *TAG = "BookViewer";

private:
  BookViewer(Fonts &fonts) : page(Page::Make(fonts)) {};

  // std::mutex mutex;
  uint16_t pageBottom;
  PageId current_page_id{-1, -1};

  PagePtr page{nullptr};

  auto recreatePage(Fonts &fonts) -> bool;
  auto buildPageAt(const PageId &pageId, EPubPtr &epub) -> void;

  struct PageEnd {
    bool operator()(Page::Format &fmt) const { return false; }
  };

public:
  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(Fonts &fonts) { return makeUniqueHimem<BookViewer>(fonts); }

  ~BookViewer() = default;

  // inline std::mutex &getMutex() { return mutex; }

  /**
   * @brief Show a page on the display.
   *
   * @param page_nbr The page number to show (First ebook page = 0, cover = -1)
   */
  auto showPage(const PageId &pageId, EPubPtr &epub) -> void;

  auto showFakeCover(EPubPtr &epub) -> void;

  static constexpr int16_t TITLE_FONT      = 2;
  static constexpr int16_t TITLE_FONT_SIZE = 8;
};
