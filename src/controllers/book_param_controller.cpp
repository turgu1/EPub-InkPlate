// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_PARAM_CONTROLLER__ 1
#include "controllers/book_param_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/book_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "controllers/common_actions.hpp"
#include "controllers/toc_controller.hpp"
#include "controllers/web_server.hpp"
#include "models/books_dir.hpp"
#include "models/config.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE_BUILD
  #include "eink.hpp"
  #include "esp.hpp"
  #include "esp_system.h"
  // #include "soc/rtc.h"
#endif

#include "book_param_controller.hpp"
#include <sys/stat.h>

static int8_t show_pictures;
static int8_t font_size;
static int8_t use_fonts_in_book;
static int8_t font;
static int8_t done_res;

static int8_t old_font_size;
static int8_t old_show_pictures;
static int8_t old_use_fonts_in_book;
static int8_t old_font;

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  static constexpr int8_t BOOK_PARAMS_FORM_SIZE = 5;
#else
  static constexpr int8_t BOOK_PARAMS_FORM_SIZE = 4;
#endif
static FormEntry book_params_form_entries[BOOK_PARAMS_FORM_SIZE] = {
    {.caption    = "Font Size:",
     .u          = {.ch = {.value        = &font_size,
                           .choice_count = 4,
                           .choices      = FormChoiceField::font_size_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
    {.caption    = "Use fonts in book:",
     .u          = {.ch = {.value        = &use_fonts_in_book,
                           .choice_count = 2,
                           .choices      = FormChoiceField::yes_no_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
    {.caption = "Font:",
     .u = {.ch = {.value = &font, .choice_count = 8, .choices = FormChoiceField::font_choices}},
     .entry_type = FormEntryType::VERTICAL},
    {.caption    = "Show Images in book:",
     .u          = {.ch = {.value        = &show_pictures,
                           .choice_count = 2,
                           .choices      = FormChoiceField::yes_no_choices}},
     .entry_type = FormEntryType::HORIZONTAL},
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    {.caption    = " DONE ",
     .u          = {.ch = {.value = &done_res, .choice_count = 0, .choices = nullptr}},
     .entry_type = FormEntryType::DONE}
#endif
};

static constexpr char const *BOOK_PARAMS_CAPTION = "Current e-book parameters";

void BookParamController::book_parameters() {
  BookParams *book_params = epub->get_book_params();

  book_params->get(BookParams::Ident::SHOW_PICTURES, &show_pictures);
  book_params->get(BookParams::Ident::FONT_SIZE, &font_size);
  book_params->get(BookParams::Ident::USE_FONTS_IN_BOOK, &use_fonts_in_book);
  book_params->get(BookParams::Ident::FONT, &font);

  if (show_pictures == -1) config.get(Config::Ident::SHOW_PICTURES, &show_pictures);
  if (font_size == -1) config.get(Config::Ident::FONT_SIZE, &font_size);
  if (use_fonts_in_book == -1) config.get(Config::Ident::USE_FONTS_IN_BOOKS, &use_fonts_in_book);
  if (font == -1) config.get(Config::Ident::DEFAULT_FONT, &font);

  old_show_pictures     = show_pictures;
  old_use_fonts_in_book = use_fonts_in_book;
  old_font              = font;
  old_font_size         = font_size;
  done_res              = 1;

  form_viewer->show(BOOK_PARAMS_CAPTION, book_params_form_entries, BOOK_PARAMS_FORM_SIZE,
                    "(Any item change will trigger book refresh)");

  book_param_controller.set_book_params_form_is_shown();
}

void BookParamController::revert_to_defaults() {
  page_locs.stop_document();

  EPub::BookFormatParams *book_format_params = epub->get_book_format_params();

  BookParams *book_params = epub->get_book_params();

  old_use_fonts_in_book = book_format_params->use_fonts_in_book;
  old_font              = book_format_params->font;

  constexpr int8_t default_value = -1;

  book_params->put(BookParams::Ident::SHOW_PICTURES, default_value);
  book_params->put(BookParams::Ident::FONT_SIZE, default_value);
  book_params->put(BookParams::Ident::FONT, default_value);
  book_params->put(BookParams::Ident::USE_FONTS_IN_BOOK, default_value);

  epub->update_book_format_params();

  book_params->save();

  MsgViewer::show(MsgViewer::MsgType::INFO, false, false, "E-book parameters reverted",
                  "E-book parameters reverted to default values.");

  if (old_use_fonts_in_book != book_format_params->use_fonts_in_book) {
    if (book_format_params->use_fonts_in_book) {
      epub->load_fonts();
    } else {
      fonts.clear();
      fonts.clear_glyph_caches();
    }
  }

  if (old_font != book_format_params->font) {
    fonts.adjust_default_font(book_format_params->font);
  }
}

void BookParamController::books_list() { app_controller.set_controller(AppController::Ctrl::DIR); }

void BookParamController::delete_book() {
  confirm_data =
      MsgViewer::show(MsgViewer::MsgType::CONFIRM, true, false, "Delete e-book",
                      "The e-book \"%s\" will be deleted. Are you sure?", epub->get_title());
  book_param_controller.set_delete_current_book();
}

void BookParamController::toc_ctrl() {
  toc_controller.set_ownership_of_book(epub);
  app_controller.set_controller(AppController::Ctrl::TOC);
}

void BookParamController::wifi_mode() {
  #if EPUB_INKPLATE_BUILD
    epub->close_file();
    fonts.clear(true);
    fonts.clear_glyph_caches();

    event_mgr.set_stay_on(true); // DO NOT sleep

    if (start_web_server()) {
      book_param_controller.set_wait_for_key_after_wifi();
    }
  #endif
}

void BookParamController::power_off() {
  books_dir_controller.save_last_book(book_controller.get_current_page_id(), true);

  CommonActions::power_it_off();
}

// IMPORTANT!!
// The first (menu[0]) and the last menu entry (the one before END_MENU) MUST ALWAYS BE VISIBLE!!!

// clang-format off
static MenuViewer::MenuEntry menu[10] = {
  {MenuViewer::Icon::RETURN,      true,  true, nullptr, "Return to the e-books reader"},
  {MenuViewer::Icon::TOC,         false, true, nullptr, "Table of Content"},
  {MenuViewer::Icon::BOOK_LIST,   true,  true, nullptr, "E-Books list"},
  {MenuViewer::Icon::FONT_PARAMS, true,  true, nullptr, BOOK_PARAMS_CAPTION},
  {MenuViewer::Icon::REVERT,      true,  true, nullptr, "Revert e-book parameters to default values"},
  {MenuViewer::Icon::DELETE,      true,  true, nullptr, "Delete the current e-book"},
  {MenuViewer::Icon::WIFI,        true,  true, nullptr, "WiFi Access to the e-books folder"},
  {MenuViewer::Icon::INFO,        true,  true, nullptr, "About the EPub-InkPlate application"},
  // This entry must be the last one before END_MENU and MUST ALWAYS BE VISIBLE!!
  {MenuViewer::Icon::POWEROFF,    true,  true, nullptr, "Power OFF (Deep Sleep)"},
  {MenuViewer::Icon::END_MENU,    false, true, nullptr, nullptr}};
// clang-format on

void BookParamController::set_font_count(uint8_t count) {
  book_params_form_entries[2].u.ch.choice_count = count;
}

void BookParamController::enter() {
  menu[1].visible = epub->toc->is_ready() && !epub->toc->is_empty();
  menu_viewer     = MenuViewer::Make();
  form_viewer     = FormViewer::Make();

  if (menu_viewer) {
    menu[0].func = CommonActions::return_to_last;
    menu[1].func = [this]() { this->toc_ctrl(); };
    menu[2].func = [this]() { this->books_list(); };
    menu[3].func = [this]() { this->book_parameters(); };
    menu[4].func = [this]() { this->revert_to_defaults(); };
    menu[5].func = [this]() { this->delete_book(); };
    menu[6].func = [this]() { this->wifi_mode(); };
    menu[7].func = CommonActions::about;
    menu[8].func = [this]() { this->power_off(); };

    menu_viewer->show(menu);
  }
  book_params_form_is_shown = false;
}

void BookParamController::leave(bool going_to_deep_sleep) {
  menu_viewer.reset();
  form_viewer.reset();
}

void BookParamController::input_event(const EventMgr::Event &event) {
  if (book_params_form_is_shown) {
    if (form_viewer->event(event)) {
      book_params_form_is_shown = false;
      // if (ok) {
      BookParams *book_params = epub->get_book_params();

      if (show_pictures != old_show_pictures)
        book_params->put(BookParams::Ident::SHOW_PICTURES, show_pictures);
      if (font_size != old_font_size) book_params->put(BookParams::Ident::FONT_SIZE, font_size);
      if (font != old_font) book_params->put(BookParams::Ident::FONT, font);
      if (use_fonts_in_book != old_use_fonts_in_book)
        book_params->put(BookParams::Ident::USE_FONTS_IN_BOOK, use_fonts_in_book);

      if (book_params->is_modified()) epub->update_book_format_params();

      book_params->save();

      if (old_use_fonts_in_book != use_fonts_in_book) {
        if (use_fonts_in_book) {
          epub->load_fonts();
        } else {
          fonts.clear();
          fonts.clear_glyph_caches();
        }
      }

      if (old_font != font) {
        fonts.adjust_default_font(font);
      }
      // }
      // menu_viewer.clear_highlight();
      menu_viewer->show(menu, 3, true);
    }
  } else if (delete_current_book) {
    if (confirm_data) {

      bool result;
      std::tie(result, confirm_data) = MsgViewer::confirm(event, std::move(confirm_data));
      if (result) {
        if (confirm_data->ok) {
          std::string filepath = epub->get_current_filename();
          struct stat file_stat;

          if (stat(filepath.c_str(), &file_stat) != -1) {
            LOG_I("Deleting %s...", filepath.c_str());

            epub->close_file();
            unlink(filepath.c_str());

            int16_t pos = filepath.find_last_of('.');

            filepath.replace(pos, 5, ".pars");

            if (stat(filepath.c_str(), &file_stat) != -1) {
              LOG_I("Deleting file : %s", filepath.c_str());
              unlink(filepath.c_str());
            }

            filepath.replace(pos, 5, ".locs");

            if (stat(filepath.c_str(), &file_stat) != -1) {
              LOG_I("Deleting file : %s", filepath.c_str());
              unlink(filepath.c_str());
            }

            filepath.replace(pos, 5, ".toc");

            if (stat(filepath.c_str(), &file_stat) != -1) {
              LOG_I("Deleting file : %s", filepath.c_str());
              unlink(filepath.c_str());
            }

            int16_t dummy;
            books_dir.refresh(nullptr, dummy, false);

            app_controller.set_controller(AppController::Ctrl::DIR);
          }
        } else {
          MsgViewer::show(MsgViewer::MsgType::INFO, false, false, "Canceled",
                          "The e-book was not deleted.");
        }
        delete_current_book = false;
      }
    }
  }
  #if EPUB_INKPLATE_BUILD
  else if (wait_for_key_after_wifi) {
      MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Restarting",
                      "The device is now restarting. Please wait.");
      wait_for_key_after_wifi = false;
      stop_web_server();
      inkplate_platform.restart();
    }
  #endif
    else {
    if (menu_viewer->event(event)) {
      book_controller.set_ownership_of_book(epub);
      app_controller.set_controller(AppController::Ctrl::BOOK);
    }
  }
}
