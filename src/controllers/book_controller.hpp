// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "viewers/book_viewer.hpp"

class BookController {
public:
  BookController() = default;

  inline void set_ownership_of_book(EPubPtr &epub_ptr) { epub = std::move(epub_ptr); }

  void input_event(const EventMgr::Event &event);
  void enter();
  void leave(bool going_to_deep_sleep = false);
  bool open_book(const std::string &book_title, const std::string &book_filename,
                 const PageId &page_id);
  // void put_str(const char *str, int xpos, int ypos);

  inline const PageId &get_current_page_id() { return current_page_id; }
  inline void set_current_page_id(const PageId &page_id) { current_page_id = page_id; }

private:
  static constexpr char const *TAG = "BookController";

  EPubPtr epub{nullptr};

  PageId current_page_id{0, 0};

  BookViewerPtr book_viewer{nullptr};
};

#if __BOOK_CONTROLLER__
  BookController book_controller;
#else
  extern BookController book_controller;
#endif
