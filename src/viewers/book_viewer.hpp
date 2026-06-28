// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

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

  /**
   * @brief Prepare a page for display.
   *
   * @param pageId The ID of the page to prepare.
   * @param epub The EPUB instance.
   * @return true if the page was successfully prepared but not displayed, false otherwise.
   * This can happen if the page is a cover page or if there was an error preparing the page.
   */
  auto preparePage(const PageId &pageId, EPubPtr &epub) -> bool;

  /**
   * @brief Display a prepared page.
   *
   * This function assumes that the page has already been prepared using preparePage().
   * It will handle the actual rendering of the page on the screen.
   * If the page was not prepared or if there was an error during preparation,
   * this function may not display anything or may display an error message.
   */
  auto displayPage(const PageId &pageId) -> void;

  auto showFakeCover(EPubPtr &epub) -> void;

  static constexpr int16_t TITLE_FONT      = 2;
  static constexpr int16_t TITLE_FONT_SIZE = 8;
};
