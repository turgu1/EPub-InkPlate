// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PARAM_CONTROLLER__ 1
#include "controllers/param_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/common_actions.hpp"
#include "models/books_dir.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/form_viewer.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE_BUILD
  #include "esp_system.h"
  #include "eink.hpp"
  #include "esp.hpp"
  #include "soc/rtc.h"
#endif

static int8_t show_images;
static int8_t font_size;
static int8_t use_fonts_in_books;
static int8_t default_font;
static int8_t ok;

static int8_t old_font_size;
static int8_t old_show_images;
static int8_t old_use_fonts_in_books;
static int8_t old_default_font;

static constexpr int8_t FONT_FORM_SIZE = 4;
static FormViewer::FormEntry font_params_form_entries[FONT_FORM_SIZE] = {
  { "Default Font Size (*):",    &font_size,              4, FormViewer::font_size_choices,   FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Use fonts in books (*):",   &use_fonts_in_books,     2, FormViewer::yes_no_choices,      FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Default font (*):",         &default_font,           8, FormViewer::font_choices,        FormViewer::FormEntryType::VERTICAL_CHOICES   },
  { nullptr,                     &ok,                     2, FormViewer::ok_cancel_choices,   FormViewer::FormEntryType::HORIZONTAL_CHOICES }
};

static void 
books_list()
{
  app_controller.set_controller(AppController::Ctrl::DIR);
}

extern bool start_web_server();
extern bool stop_web_server();

static void
wifi_mode()
{
  #if EPUB_INKPLATE_BUILD
    epub.close_file();
    fonts.clear();
    fonts.clear_glyph_caches();
    
    event_mgr.set_stay_on(true); // DO NOT sleep

    if (start_web_server()) {
      param_controller.set_wait_for_key_after_wifi();
    }
  #endif
}

static MenuViewer::MenuEntry menu[6] = {
  { MenuViewer::Icon::RETURN,    "Return to the e-books reader",        CommonActions::return_to_last    },
  { MenuViewer::Icon::BOOK_LIST, "E-Books list",                        books_list                       },
  { MenuViewer::Icon::WIFI,      "WiFi Access to the e-books folder",   wifi_mode                        },
  { MenuViewer::Icon::INFO,      "About the EPub-InkPlate application", CommonActions::about             },
  { MenuViewer::Icon::POWEROFF,  "Power OFF (Deep Sleep)",              CommonActions::power_off         },
  { MenuViewer::Icon::END_MENU,  nullptr,                               nullptr                          }
}; 

void 
ParamController::enter()
{
  menu_viewer.show(menu);
}

void 
ParamController::leave(bool going_to_deep_sleep)
{

}

void 
ParamController::key_event(EventMgr::KeyEvent key)
{
  if (form_is_shown) {
    if (form_viewer.event(key)) {
      form_is_shown = false;
    }
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
    if (menu_viewer.event(key)) {
      app_controller.set_controller(AppController::Ctrl::LAST);
    }
  }
}