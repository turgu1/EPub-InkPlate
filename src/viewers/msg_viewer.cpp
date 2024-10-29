// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MSG_VIEWER__ 1
#include "viewers/msg_viewer.hpp"

#include "viewers/book_viewer.hpp"
#include "viewers/page.hpp"
#include "screen.hpp"

#include <cstdarg>

#if EPUB_INKPLATE_BUILD
  #include "nvs.h"
  #include "inkplate_platform.hpp"
#endif

char MsgViewer::icon_char[7] = { 'I',  '!', 'H', 'E', 'S', 'Y', '!' };

void MsgViewer::show(
  MsgType msg_type, 
  bool press_a_key, 
  bool clear_screen,
  const char * title, 
  const char * fmt_str, ...)
{
  char buff[250];

  width = Screen::get_width() - 60;

  if (page.get_compute_mode() == Page::ComputeMode::LOCATION) return; // Cannot be used durint location computation

  va_list args;
  va_start(args, fmt_str);
  vsnprintf(buff, 250, fmt_str, args);
  va_end(args);

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   0,
    .font_size          =  24,
    .indent             =   0,
    .margin_left        =  10,
    .margin_right       =  10,
    .margin_top         =  30,
    .margin_bottom      =  10,
    .screen_left        =   0,
    .screen_right       =   0,
    .screen_top         =   0,
    .screen_bottom      =   0,
    .width              =   0,
    .height             =   0,
    .vertical_align     =   0,
    .trim               = true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = CSS::Align::CENTER,
    .text_transform     = CSS::TextTransform::NONE,
    .display            = CSS::Display::INLINE
  };

  fmt.screen_left        = (Screen::get_width()  - width ) >> 1;
  fmt.screen_right       = (Screen::get_width()  - width ) >> 1;
  fmt.screen_top         = (Screen::get_height() - HEIGHT) >> 1;
  fmt.screen_bottom      = (Screen::get_height() - HEIGHT) >> 1;

  page.set_compute_mode(Page::ComputeMode::DISPLAY);
  
  page.start(fmt);

  page.clear_region(
    Dim(width, HEIGHT), 
    Pos((Screen::get_width()  - width ) >> 1, (Screen::get_height() - HEIGHT) >> 1));

  page.put_highlight(
    Dim(width - 4, HEIGHT - 4), 
    Pos(((Screen::get_width() - width ) >> 1) + 2, ((Screen::get_height() - HEIGHT) >> 1) + 2));

  Font * font = fonts.get(0);

  if (font == nullptr) {
    LOG_E("Internal error (Drawings Font not available!");
    return;
  }

  Font::Glyph * glyph = font->get_glyph(icon_char[msg_type], 24);
  
  if (glyph != nullptr) {
    page.put_char_at(
      icon_char[msg_type], 
      Pos(((Screen::get_width()  - width ) >> 1) + 50 - (glyph->dim.width >> 1), ( Screen::get_height() >> 1) + 20),
      fmt);
  }

  fmt.font_index =  1;
  fmt.font_size  = 10;

  // Title

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  std::string buffer = title;
  page.add_text(buffer, fmt);
  page.end_paragraph(fmt);

  // Message

  fmt.align       = CSS::Align::LEFT;
  fmt.margin_top  = 80;
  fmt.margin_left = 100;

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  buffer = buff;
  page.add_text(buffer, fmt);
  page.end_paragraph(fmt);

  // Press a Key option

  if (press_a_key) {
    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      if (msg_type != MsgType::CONFIRM) {
        fmt.align       = CSS::Align::CENTER;
        fmt.font_size   =                  9;
        fmt.margin_left =                 10;
        fmt.margin_top  =                200;

        page.set_limits(fmt);
        page.new_paragraph(fmt);
        buffer = "[Please TAP the screen]";
        page.add_text(buffer, fmt);
        page.end_paragraph(fmt);
      } 
      else {
        font = fonts.get(1);

        if (font == nullptr) {
          LOG_E("Internal error (Main Font not available!");
          return;
        }

        Dim dim, ok_dim;
        font->get_size("CANCEL", &dim,    10); 
        font->get_size("OK",     &ok_dim, 10); 
        buttons_dim = Dim(dim.width + 20, dim.height + 20);
        ok_pos = Pos((Screen::get_width() >> 1) - 20 - buttons_dim.width, fmt.screen_top + 220);
        cancel_pos = Pos((Screen::get_width() >> 1) + 20, fmt.screen_top + 220);

        page.put_rounded(buttons_dim, ok_pos);
        page.put_rounded(Dim(buttons_dim.width + 2, buttons_dim.height + 2),
                         Pos(ok_pos.x - 1,          ok_pos.y - 1));
        page.put_rounded(Dim(buttons_dim.width + 4, buttons_dim.height + 4),
                         Pos(ok_pos.x - 2,          ok_pos.y - 2));

        page.put_str_at("OK", Pos(ok_pos.x + (buttons_dim.width  >> 1) - (ok_dim.width  >> 1),
                                  ok_pos.y + (buttons_dim.height >> 1) + (ok_dim.height >> 1)), fmt);

        page.put_rounded(buttons_dim, cancel_pos);
        page.put_rounded(Dim(buttons_dim.width + 2, buttons_dim.height + 2),
                         Pos(cancel_pos.x - 1,      cancel_pos.y - 1));
        page.put_rounded(Dim(buttons_dim.width + 4, buttons_dim.height + 4),
                         Pos(cancel_pos.x - 2,      cancel_pos.y - 2));

        page.put_str_at("CANCEL", Pos(cancel_pos.x + (buttons_dim.width  >> 1) - (dim.width  >> 1),
                                      cancel_pos.y + (buttons_dim.height >> 1) + (dim.height >> 1)), fmt);
      }
    #else
      fmt.align       = CSS::Align::CENTER;
      fmt.font_size   =                  9;
      fmt.margin_left =                 10;
      fmt.margin_top  =                200;

      page.set_limits(fmt);
      page.new_paragraph(fmt);
      buffer = msg_type == MsgType::CONFIRM ? "[Press SELECT to Confirm]" : "[Press any key]";
      page.add_text(buffer, fmt);
      page.end_paragraph(fmt);
    #endif
  }

  page.paint(clear_screen, !clear_screen, true);
}

bool MsgViewer::confirm(const EventMgr::Event & event, bool & ok)
{
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL

    if (event.kind == EventMgr::EventKind::TAP) {
      if ((event.x >= ok_pos.x) && (event.x <= (ok_pos.x + buttons_dim.width )) &&
          (event.y >= ok_pos.y) && (event.y <= (ok_pos.y + buttons_dim.height))) {
        ok = true;
        return true;
      }
      else if ((event.x >= cancel_pos.x) && (event.x <= (cancel_pos.x + buttons_dim.width )) &&
               (event.y >= cancel_pos.y) && (event.y <= (cancel_pos.y + buttons_dim.height))) {
        ok = false;
        return true;
      }
    }
    return false;
  #else 
    ok = event.kind == EventMgr::EventKind::SELECT;
    return true;
  #endif

}

#if 0
void MsgViewer::show_progress(const char * title, ...)
{
  width = Screen::get_width() - 60;

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   1,
    .font_size          =  10,
    .indent             =   0,
    .margin_left        =  10,
    .margin_right       =  10,
    .margin_top         =  30, // 70,
    .margin_bottom      =  10,
    .screen_left        =   0,
    .screen_right       =   0,
    .screen_top         =   0,
    .screen_bottom      =   0,
    .width              =   0,
    .height             =   0,
    .trim               = true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = CSS::Align::CENTER,
    .vertical_align     = 0,
    .text_transform     = CSS::TextTransform::NONE,
    .display            = CSS::Display::INLINE
  };

  fmt.screen_left        = (Screen::get_width()  - width  ) >> 1;
  fmt.screen_right       = (Screen::get_width()  - width  ) >> 1;
  fmt.screen_top         = (Screen::get_height() - HEIGHT2) >> 1;
  fmt.screen_bottom      = (Screen::get_height() - HEIGHT2) >> 1;

  char buff[80];

  va_list args;
  va_start(args, title);
  vsnprintf(buff, 80, title, args);
  va_end(args);

  page.start(fmt);

  page.clear_region(
    Dim(width, HEIGHT2), 
    Pos((Screen::get_width()  - width  ) >> 1, (Screen::get_height() - HEIGHT2) >> 1));

  page.put_highlight(
    Dim(width - 4, HEIGHT2 - 4), 
    Pos(((Screen::get_width() - width  ) >> 1) + 2, ((Screen::get_height() - HEIGHT2) >> 1) + 2));

  // Title

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  std::string buffer = buff;
  page.add_text(buffer, fmt);
  page.end_paragraph(fmt);

  // Progress zone

  page.put_highlight(
    Dim(width - 42, HEIGHT2 - 100), 
    Pos(((Screen::get_width() - width) >> 1) +  23, (Screen::get_height() >> 1) - 120)
  );

  dot_zone.dim  = Dim(width -  46, HEIGHT2 - 104);
  dot_zone.pos  = Pos(((Screen::get_width() - width) >> 1) +  25, (Screen::get_height() >> 1) - 118);
  dot_zone.dots_per_line = (dot_zone.dim.width + 1) / 9;
  dot_zone.max_dot_count = dot_zone.dots_per_line * ((dot_zone.dim.height + 1) / 9);
  dot_count = 0;

  page.paint(false);
}

void MsgViewer::add_dot()
{
  width = Screen::get_width() - 60;

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   1,
    .font_size          =  10,
    .indent             =   0,
    .margin_left        =  10,
    .margin_right       =  10,
    .margin_top         =  30, // 70,
    .margin_bottom      =  10,
    .screen_left        =   0,
    .screen_right       =   0,
    .screen_top         =   0,
    .screen_bottom      =   0,
    .width              =   0,
    .height             =   0,
    .trim               = true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = CSS::Align::CENTER,
    .vertical_align     = 0,
    .text_transform     = CSS::TextTransform::NONE,
    .display            = CSS::Display::INLINE
  };

  fmt.screen_left        = (Screen::get_width()  - width ) >> 1;
  fmt.screen_right       = (Screen::get_width()  - width ) >> 1;
  fmt.screen_top         = (Screen::get_height() - HEIGHT) >> 1;
  fmt.screen_bottom      = (Screen::get_height() - HEIGHT) >> 1;

  page.start(fmt);

  if (dot_count >= dot_zone.max_dot_count) {
    page.clear_region(dot_zone.dim, dot_zone.pos);
    dot_count = 0;
  }

  Pos pos(dot_zone.pos.x + (dot_count % dot_zone.dots_per_line) * 9,
          dot_zone.pos.y + (dot_count / dot_zone.dots_per_line) * 9);

  page.set_region(Dim{ 8, 8 }, pos);

  dot_count++;

  page.paint(false, true, true);
}
#endif

void 
MsgViewer::out_of_memory(const char * raison)
{
  #if EPUB_INKPLATE_BUILD
    nvs_handle_t nvs_handle;
    esp_err_t    err;

    if ((err = nvs_open("EPUB-InkPlate", NVS_READWRITE, &nvs_handle)) == ESP_OK) {
      if (nvs_erase_all(nvs_handle) == ESP_OK) {
        nvs_commit(nvs_handle);
      }
      nvs_close(nvs_handle);
    }
  #endif

  screen.force_full_update();

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    #define MSG "Press the WakeUp Button to restart."
    #define INT_PIN TouchScreen::INTERRUPT_PIN
    #define LEVEL 0
  #else
    #define MSG "Press a key to restart."
    #if EXTENDED_CASE
      #define INT_PIN PressKeys::INTERRUPT_PIN
    #else
      #define INT_PIN TouchKeys::INTERRUPT_PIN
    #endif
    #define LEVEL 1
  #endif
  
  show(ALERT, true, true, "OUT OF MEMORY!!",
    "It's a bit sad that the device is now out of "
    "memory to continue. The reason: %s. "
    "The device is now entering into Deep Sleep. "
    MSG,
    raison
  );
 
  #undef MSG

  #if EPUB_INKPLATE_BUILD
    inkplate_platform.deep_sleep(INT_PIN, LEVEL); // Never return
  #else
    exit(0);
  #endif
}
