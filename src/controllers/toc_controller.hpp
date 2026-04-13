// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "viewers/toc_viewer.hpp"

class TocController {
private:
  static constexpr char const *TAG = "TocController";

  int16_t current_entry_index;
  int16_t current_book_index;

  TocViewerPtr toc_viewer{nullptr};

  EPubPtr epub{nullptr};

public:
  TocController() : current_entry_index(-1), current_book_index(-1) {}

  inline void set_ownership_of_book(EPubPtr &epub_ptr) { epub = std::move(epub_ptr); }

  void input_event(const EventMgr::Event &event);
  void enter();
  void leave(bool going_to_deep_sleep = false);
};

#if __TOC_CONTROLLER__
  TocController toc_controller;
#else
  extern TocController toc_controller;
#endif
