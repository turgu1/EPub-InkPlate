// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "viewers/books_dir_viewer.hpp"
#include "viewers/linear_books_dir_viewer.hpp"
#include "viewers/matrix_books_dir_viewer.hpp"

class BooksDirController {
private:
  static constexpr char const *TAG = "BooksDirController";

  const uint8_t LINEAR_VIEWER = 0;
  const uint8_t MATRIX_VIEWER = 1;

  int32_t book_offset{0};
  int16_t current_book_index{0};
  int16_t last_read_book_index{0};
  std::string book_filename;
  bool book_was_shown{false};

  PageId book_page_id{0, 0};
  BooksDirViewerPtr books_dir_viewer{nullptr};

  int8_t viewer_id{0};

public:
  BooksDirController() = default;
  void setup();
  void input_event(const EventMgr::Event &event);
  void enter();
  void leave(bool going_to_deep_sleep = false);
  void save_last_book(const PageId &page_id, bool going_to_deep_sleep);
  void show_last_book();
  void new_orientation() {
    if (books_dir_viewer != nullptr) books_dir_viewer->setup();
  }

  inline int16_t get_current_book_index() { return current_book_index; }
  inline void set_current_book_index(int16_t idx) { current_book_index = idx; }
};

#if __BOOKS_DIR_CONTROLLER__
  BooksDirController books_dir_controller;
#else
  extern BooksDirController books_dir_controller;
#endif
