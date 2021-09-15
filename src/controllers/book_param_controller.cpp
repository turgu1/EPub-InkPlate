// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_PARAM_CONTROLLER__ 1
#include "controllers/book_param_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/common_actions.hpp"
#include "models/books_dir.hpp"
#include "models/epub.hpp"
#include "models/config.hpp"
#include "models/page_locs.hpp"
#include "models/toc.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/form_viewer.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE_BUILD
  #include "esp_system.h"
  #include "eink.hpp"
  #include "esp.hpp"
  #include "soc/rtc.h"
#endif

#include <sys/stat.h>

static int8_t show_images;
static int8_t font_size;
static int8_t use_fonts_in_book;
static int8_t font;
static int8_t done;

static int8_t old_font_size;
static int8_t old_show_images;
static int8_t old_use_fonts_in_book;
static int8_t old_font;

#if INKPLATE_6PLUS || TOUCH_TRIAL
  static constexpr int8_t BOOK_PARAMS_FORM_SIZE = 5;
#else
  static constexpr int8_t BOOK_PARAMS_FORM_SIZE = 4;
#endif
static FormViewer::FormEntry book_params_form_entries[BOOK_PARAMS_FORM_SIZE] = {
  { "Font Size:",           &font_size,          4, FormViewer::font_size_choices, FormViewer::FormEntryType::HORIZONTAL },
  { "Use fonts in book:",   &use_fonts_in_book,  2, FormViewer::yes_no_choices,    FormViewer::FormEntryType::HORIZONTAL },
  { "Font:",                &font,               8, FormViewer::font_choices,      FormViewer::FormEntryType::VERTICAL   },
  { "Show Images in book:", &show_images,        2, FormViewer::yes_no_choices,    FormViewer::FormEntryType::HORIZONTAL },
  #if INKPLATE_6PLUS || TOUCH_TRIAL
    { nullptr,              &done,               1, FormViewer::done_choices,      FormViewer::FormEntryType::DONE       }
  #endif
};

static void
book_parameters()
{
  BookParams * book_params = epub.get_book_params();

  book_params->get(BookParams::Ident::SHOW_IMAGES,        &show_images      );
  book_params->get(BookParams::Ident::FONT_SIZE,          &font_size        );
  book_params->get(BookParams::Ident::USE_FONTS_IN_BOOK,  &use_fonts_in_book);
  book_params->get(BookParams::Ident::FONT,               &font             );
  
  if (show_images       == -1) config.get(Config::Ident::SHOW_IMAGES,        &show_images      );
  if (font_size         == -1) config.get(Config::Ident::FONT_SIZE,          &font_size        );
  if (use_fonts_in_book == -1) config.get(Config::Ident::USE_FONTS_IN_BOOKS, &use_fonts_in_book);
  if (font              == -1) config.get(Config::Ident::DEFAULT_FONT,       &font             );
  
  old_show_images        = show_images;
  old_use_fonts_in_book  = use_fonts_in_book;
  old_font               = font;
  old_font_size          = font_size;
  done                   = 1;

  form_viewer.show(
    book_params_form_entries, 
    BOOK_PARAMS_FORM_SIZE, 
    "(Any item change will trigger book refresh)");

  book_param_controller.set_book_params_form_is_shown();
}

static void
revert_to_defaults()
{
  page_locs.stop_document();
  
  EPub::BookFormatParams * book_format_params = epub.get_book_format_params();

  BookParams * book_params = epub.get_book_params();

  old_use_fonts_in_book = book_format_params->use_fonts_in_book;
  old_font              = book_format_params->font;

  constexpr int8_t default_value = -1;

  book_params->put(BookParams::Ident::SHOW_IMAGES,       default_value);
  book_params->put(BookParams::Ident::FONT_SIZE,         default_value);
  book_params->put(BookParams::Ident::FONT,              default_value);
  book_params->put(BookParams::Ident::USE_FONTS_IN_BOOK, default_value);
  
  epub.update_book_format_params();

  book_params->save();

  msg_viewer.show(MsgViewer::INFO, 
                  false, false, 
                  "E-book parameters reverted", 
                  "E-book parameters reverted to default values.");

  if (old_use_fonts_in_book != book_format_params->use_fonts_in_book) {
    if (book_format_params->use_fonts_in_book) {
      epub.load_fonts();
    }
    else {
      fonts.clear();
      fonts.clear_glyph_caches();
    }
  }

  if (old_font != book_format_params->font) {
    fonts.adjust_default_font(book_format_params->font);
  }
}

static void 
books_list()
{
  app_controller.set_controller(AppController::Ctrl::DIR);
}

static void
delete_book()
{
  msg_viewer.show(MsgViewer::Severity::CONFIRM, true, false,
                  "Delete e-book", 
                  "The e-book \"%s\" will be deleted. Are you sure?", 
                  epub.get_title());
  book_param_controller.set_delete_current_book();
}

static void 
toc_ctrl()
{
  app_controller.set_controller(AppController::Ctrl::TOC);
}

extern bool start_web_server();
extern bool  stop_web_server();

static void
wifi_mode()
{
  #if EPUB_INKPLATE_BUILD
    epub.close_file();
    fonts.clear();
    fonts.clear_glyph_caches();
    
    event_mgr.set_stay_on(true); // DO NOT sleep

    if (start_web_server()) {
      book_param_controller.set_wait_for_key_after_wifi();
    }
  #endif
}

// IMPORTANT!!
// The first (menu[0]) and the last menu entry (the one before END_MENU) MUST ALWAYS BE VISIBLE!!!

static MenuViewer::MenuEntry menu[10] = {
  { MenuViewer::Icon::RETURN,      "Return to the e-books reader",         CommonActions::return_to_last, true  },
  { MenuViewer::Icon::TOC,         "Table of Content",                     toc_ctrl                     , false },
  { MenuViewer::Icon::BOOK_LIST,   "E-Books list",                         books_list                   , true  },
  { MenuViewer::Icon::FONT_PARAMS, "Current e-book parameters",            book_parameters              , true  },
  { MenuViewer::Icon::REVERT,      "Revert e-book parameters to "
                                   "default values",                       revert_to_defaults           , true  },  
  { MenuViewer::Icon::DELETE,      "Delete the current e-book",            delete_book                  , true  },
  { MenuViewer::Icon::WIFI,        "WiFi Access to the e-books folder",    wifi_mode                    , true  },
  { MenuViewer::Icon::INFO,        "About the EPub-InkPlate application",  CommonActions::about         , true  },
  { MenuViewer::Icon::POWEROFF,    "Power OFF (Deep Sleep)",               CommonActions::power_off     , true  },
  { MenuViewer::Icon::END_MENU,    nullptr,                                nullptr                      , false }
}; 

void 
BookParamController::enter()
{
  menu[1].visible = toc.is_ready() && !toc.is_empty();
  menu_viewer.show(menu);
  book_params_form_is_shown = false;
}

void 
BookParamController::leave(bool going_to_deep_sleep)
{

}

void 
BookParamController::input_event(EventMgr::Event event)
{
  if (book_params_form_is_shown) {
    if (form_viewer.event(event)) {
      book_params_form_is_shown = false;
      // if (ok) {
        BookParams * book_params = epub.get_book_params();

        if (show_images       !=       old_show_images) book_params->put(BookParams::Ident::SHOW_IMAGES,        show_images      );
        if (font_size         !=         old_font_size) book_params->put(BookParams::Ident::FONT_SIZE,          font_size        );
        if (font              !=              old_font) book_params->put(BookParams::Ident::FONT,               font             );
        if (use_fonts_in_book != old_use_fonts_in_book) book_params->put(BookParams::Ident::USE_FONTS_IN_BOOK,  use_fonts_in_book);
        
        if (book_params->is_modified()) epub.update_book_format_params();

        book_params->save();

        if (old_use_fonts_in_book != use_fonts_in_book) {
          if (use_fonts_in_book) {
            epub.load_fonts();
          }
          else {
            fonts.clear();
            fonts.clear_glyph_caches();
          }
        }
 
        if (old_font != font) {
          fonts.adjust_default_font(font);
        }
     // }
      menu_viewer.clear_highlight();
    }
  }
  else if (delete_current_book) {
    if (event == EventMgr::Event::SELECT) {
      std::string filepath = epub.get_current_filename();
      struct stat file_stat;

      if (stat(filepath.c_str(), &file_stat) != -1) {
        LOG_I("Deleting %s...", filepath.c_str());

        epub.close_file();
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
    }
    else {
      msg_viewer.show(MsgViewer::Severity::INFO, false, false, 
                      "Canceled", "The e-book was not deleted.");
    }
    delete_current_book = false;
  }
  #if EPUB_INKPLATE_BUILD
    else if (wait_for_key_after_wifi) {
      msg_viewer.show(MsgViewer::INFO, 
                      false, true, 
                      "Restarting", 
                      "The device is now restarting. Please wait.");
      wait_for_key_after_wifi = false;
      stop_web_server();
      esp_restart();
    }
  #endif
  else {
    if (menu_viewer.event(event)) {
      app_controller.set_controller(AppController::Ctrl::LAST);
    }
  }
}