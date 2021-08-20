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

char MsgViewer::icon_char[5] = { 'I',  '!', 'H', 'E', 'S' };

void MsgViewer::show(
  Severity severity, 
  bool press_a_key, 
  bool clear_screen,
  const char * title, 
  const char * fmt_str, ...)
{
  char buff[200];

  width = Screen::WIDTH - 60;

  if (page.get_compute_mode() == Page::ComputeMode::LOCATION) return; // Cannot be used durint location computation

  va_list args;
  va_start(args, fmt_str);
  vsnprintf(buff, 200, fmt_str, args);
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

  fmt.screen_left        = (Screen::WIDTH  - width ) >> 1;
  fmt.screen_right       = (Screen::WIDTH  - width ) >> 1;
  fmt.screen_top         = (Screen::HEIGHT - HEIGHT) >> 1;
  fmt.screen_bottom      = (Screen::HEIGHT - HEIGHT) >> 1;

  page.set_compute_mode(Page::ComputeMode::DISPLAY);
  
  page.start(fmt);

  page.clear_region(
    Dim(width, HEIGHT), 
    Pos((Screen::WIDTH  - width ) >> 1, (Screen::HEIGHT - HEIGHT) >> 1));

  page.put_highlight(
    Dim(width - 4, HEIGHT - 4), 
    Pos(((Screen::WIDTH - width ) >> 1) + 2, ((Screen::HEIGHT - HEIGHT) >> 1) + 2));

  TTF * font = fonts.get(0);
  TTF::BitmapGlyph * glyph = font->get_glyph(icon_char[severity], 24);

  page.put_char_at(
    icon_char[severity], 
    Pos(((Screen::WIDTH  - width ) >> 1) + 50 - (glyph->dim.width >> 1), ( Screen::HEIGHT >> 1) + 20),
    fmt);

  fmt.font_index =  5;
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
  fmt.margin_left = 90;

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  buffer = buff;
  page.add_text(buffer, fmt);
  page.end_paragraph(fmt);

  // Press a Key option

  if (press_a_key) {
    fmt.align = CSS::Align::CENTER;
    fmt.font_size   =   9;
    fmt.margin_left =  10;
    fmt.margin_top  = 200;

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    buffer = "[Press a key]";
    page.add_text(buffer, fmt);
    page.end_paragraph(fmt);
  }

  page.paint(clear_screen, true, true);
}

#if 0
void MsgViewer::show_progress(const char * title, ...)
{
  width = Screen::WIDTH - 60;

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   5,
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

  fmt.screen_left        = (Screen::WIDTH  - width  ) >> 1;
  fmt.screen_right       = (Screen::WIDTH  - width  ) >> 1;
  fmt.screen_top         = (Screen::HEIGHT - HEIGHT2) >> 1;
  fmt.screen_bottom      = (Screen::HEIGHT - HEIGHT2) >> 1;

  char buff[80];

  va_list args;
  va_start(args, title);
  vsnprintf(buff, 80, title, args);
  va_end(args);

  page.start(fmt);

  page.clear_region(
    Dim(width, HEIGHT2), 
    Pos((Screen::WIDTH  - width  ) >> 1, (Screen::HEIGHT - HEIGHT2) >> 1));

  page.put_highlight(
    Dim(width - 4, HEIGHT2 - 4), 
    Pos(((Screen::WIDTH - width  ) >> 1) + 2, ((Screen::HEIGHT - HEIGHT2) >> 1) + 2));

  // Title

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  std::string buffer = buff;
  page.add_text(buffer, fmt);
  page.end_paragraph(fmt);

  // Progress zone

  page.put_highlight(
    Dim(width - 42, HEIGHT2 - 100), 
    Pos(((Screen::WIDTH - width) >> 1) +  23, (Screen::HEIGHT >> 1) - 120)
  );

  dot_zone.dim  = Dim(width -  46, HEIGHT2 - 104);
  dot_zone.pos  = Pos(((Screen::WIDTH - width) >> 1) +  25, (Screen::HEIGHT >> 1) - 118);
  dot_zone.dots_per_line = (dot_zone.dim.width + 1) / 9;
  dot_zone.max_dot_count = dot_zone.dots_per_line * ((dot_zone.dim.height + 1) / 9);
  dot_count = 0;

  page.paint(false);
}

void MsgViewer::add_dot()
{
  width = Screen::WIDTH - 60;

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

  fmt.screen_left        = (Screen::WIDTH  - width ) >> 1;
  fmt.screen_right       = (Screen::WIDTH  - width ) >> 1;
  fmt.screen_top         = (Screen::HEIGHT - HEIGHT) >> 1;
  fmt.screen_bottom      = (Screen::HEIGHT - HEIGHT) >> 1;

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

  show(ALERT, true, true, "OUT OF MEMORY!!",
    "It's a bit sad that the device is now out of "
    "memory to continue. The reason: %s. "
    "The device is now entering into Deep Sleep. "
    "Press any key to restart.",
    raison
  );

  #if EPUB_INKPLATE_BUILD
    inkplate_platform.deep_sleep(); // Never return
  #else
    exit(0);
  #endif
}