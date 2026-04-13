// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/book_controller.hpp"
#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "viewers/form_viewer.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/msg_viewer.hpp"

class BookParamController {
private:
  static constexpr char const *TAG = "BookParamController";

  bool book_params_form_is_shown{false};
  bool wait_for_key_after_wifi{false};
  bool delete_current_book{false};

  EPubPtr epub{nullptr};

public:
  BookParamController() = default;

  inline void set_ownership_of_book(EPubPtr &epub_ptr) { epub = std::move(epub_ptr); }

  MenuViewerPtr menu_viewer;
  FormViewerPtr form_viewer;

  MsgViewer::ConfirmDataPtr confirm_data;

  void input_event(const EventMgr::Event &event);
  void enter();
  void leave(bool going_to_deep_sleep = false);
  void set_font_count(uint8_t count);

  inline void set_book_params_form_is_shown() { book_params_form_is_shown = true; }
  inline void set_wait_for_key_after_wifi() { wait_for_key_after_wifi = true; }
  inline void set_delete_current_book() { delete_current_book = true; }

  void book_parameters();
  void revert_to_defaults();
  void books_list();
  void delete_book();
  void toc_ctrl();
  void wifi_mode();
  void power_off();
};

#if __BOOK_PARAM_CONTROLLER__
  BookParamController book_param_controller;
#else
  extern BookParamController book_param_controller;
#endif
