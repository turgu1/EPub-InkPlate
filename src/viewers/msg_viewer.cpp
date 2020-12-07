// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MSG_VIEWER__ 1
#include "viewers/msg_viewer.hpp"

#include "viewers/page.hpp"
#include "screen.hpp"

#include <cstdarg>

#if EPUB_INKPLATE6_BUILD
  #include "nvs.h"
  #include "inkplate6_ctrl.hpp"
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

  if (page.get_compute_mode() == Page::LOCATION) return; // Cannot be used durint location computation

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
    .trim               = true,
    .pre                = false,
    .font_style         = Fonts::NORMAL,
    .align              = CSS::CENTER_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM,
    .display            = CSS::INLINE
  };

  fmt.screen_left        = (Screen::WIDTH  - width ) >> 1;
  fmt.screen_right       = (Screen::WIDTH  - width ) >> 1;
  fmt.screen_top         = (Screen::HEIGHT - HEIGHT) >> 1;
  fmt.screen_bottom      = (Screen::HEIGHT - HEIGHT) >> 1;

  page.set_compute_mode(Page::DISPLAY);
  
  page.start(fmt);

  page.clear_region(
    width, 
    HEIGHT, 
    (Screen::WIDTH  - width ) >> 1, 
    (Screen::HEIGHT - HEIGHT) >> 1);

  page.put_highlight(
    width  - 4, 
    HEIGHT - 4, 
    ((Screen::WIDTH  - width ) >> 1) + 2,
    ((Screen::HEIGHT - HEIGHT) >> 1) + 2);

  TTF * font = fonts.get(0, 24);
  TTF::BitmapGlyph * glyph = font->get_glyph(icon_char[severity]);

  page.put_char_at(
    icon_char[severity], 
    ((Screen::WIDTH  - width ) >> 1) + 50 - (glyph->width >> 1), 
    ( Screen::HEIGHT           >> 1) + 20,
    fmt);

  fmt.font_index =  1;
  fmt.font_size  = 10;

  // Title

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  std::string buffer = title;
  page.put_text(buffer, fmt);
  page.end_paragraph(fmt);

  // Message

  fmt.align       = CSS::LEFT_ALIGN;
  fmt.margin_top  = 80;
  fmt.margin_left = 90;

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  buffer = buff;
  page.put_text(buffer, fmt);
  page.end_paragraph(fmt);

  // Press a Key option

  if (press_a_key) {
    fmt.align = CSS::CENTER_ALIGN;
    fmt.font_size   =   9;
    fmt.margin_left =  10;
    fmt.margin_top  = 200;

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    buffer = "[Press a key]";
    page.put_text(buffer, fmt);
    page.end_paragraph(fmt);
  }

  page.paint(clear_screen, true, true);
}

void MsgViewer::show_progress(const char * title, ...)
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
    .font_style         = Fonts::NORMAL,
    .align              = CSS::CENTER_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM,
    .display            = CSS::INLINE
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
    width, 
    HEIGHT2, 
    (Screen::WIDTH  - width  ) >> 1, 
    (Screen::HEIGHT - HEIGHT2) >> 1);

  page.put_highlight(
    width   - 4, 
    HEIGHT2 - 4, 
    ((Screen::WIDTH  - width  ) >> 1) + 2,
    ((Screen::HEIGHT - HEIGHT2) >> 1) + 2);

  // Title

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  std::string buffer = buff;
  page.put_text(buffer, fmt);
  page.end_paragraph(fmt);

  // Progress zone

  page.put_highlight(
    width   -   42,
    HEIGHT2 -  100,
    ((Screen::WIDTH - width) >> 1) +  23,
     (Screen::HEIGHT         >> 1) - 120
  );

  dot_zone.width  = width   -  46;
  dot_zone.height = HEIGHT2 - 104;
  dot_zone.xpos   = ((Screen::WIDTH - width) >> 1) +  25;
  dot_zone.ypos   =  (Screen::HEIGHT         >> 1) - 118;
  dot_zone.dots_per_line = (dot_zone.width + 1) / 9;
  dot_zone.max_dot_count = dot_zone.dots_per_line * ((dot_zone.height + 1) / 9);
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
    .font_style         = Fonts::NORMAL,
    .align              = CSS::CENTER_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM,
    .display            = CSS::INLINE
  };

  fmt.screen_left        = (Screen::WIDTH  - width ) >> 1;
  fmt.screen_right       = (Screen::WIDTH  - width ) >> 1;
  fmt.screen_top         = (Screen::HEIGHT - HEIGHT) >> 1;
  fmt.screen_bottom      = (Screen::HEIGHT - HEIGHT) >> 1;

  page.start(fmt);

  if (dot_count >= dot_zone.max_dot_count) {
    page.clear_region(dot_zone.width, dot_zone.height, dot_zone.xpos, dot_zone.ypos);
    dot_count = 0;
  }

  int16_t xpos = dot_zone.xpos + (dot_count % dot_zone.dots_per_line) * 9;
  int16_t ypos = dot_zone.ypos + (dot_count / dot_zone.dots_per_line) * 9;

  page.set_region(8, 8, xpos, ypos);

  dot_count++;

  page.paint(false, true, true);
}

void 
MsgViewer::out_of_memory(const char * raison)
{
  #if EPUB_INKPLATE6_BUILD
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

  #if EPUB_INKPLATE6_BUILD
    inkplate6_ctrl.deep_sleep(); // Never return
  #else
    exit(0);
  #endif
}