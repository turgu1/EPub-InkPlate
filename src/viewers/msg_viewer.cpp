// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "viewers/msg_viewer.hpp"

#include "viewers/page.hpp"
#include "screen.hpp"

#include <cstdarg>

char MsgViewer::icon_char[4] = { 'I',  '!', 'H', 'E' };

void MsgViewer::show(
  Severity severity, 
  bool press_a_key, 
  const char * title, 
  const char * fmt_str, ...)
{
  char buff[200];

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
    .margin_top         =  30, // 70,
    .margin_bottom      =  10,
    .screen_left        = (Screen::WIDTH  -   WIDTH ) >> 1,
    .screen_right       = (Screen::WIDTH  -   WIDTH ) >> 1,
    .screen_top         = (Screen::HEIGHT -   HEIGHT) >> 1,
    .screen_bottom      = (Screen::HEIGHT -   HEIGHT) >> 1,
    .width              = 0,
    .height             = 0,
    .trim               = true,
    .font_style         = Fonts::NORMAL,
    .align              = CSS::CENTER_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM
  };

  page.start(fmt);

  page.clear_region(
    WIDTH, 
    HEIGHT, 
    (Screen::WIDTH  - WIDTH ) >> 1, 
    (Screen::HEIGHT - HEIGHT) >> 1);

  page.put_highlight(
    WIDTH  - 4, 
    HEIGHT - 4, 
    ((Screen::WIDTH  - WIDTH ) >> 1) + 2,
    ((Screen::HEIGHT - HEIGHT) >> 1) + 2);

  page.put_char_at(
    icon_char[severity], 
    ((Screen::WIDTH  - WIDTH ) >> 1) + 20, 
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

  page.paint(false);
}

void MsgViewer::show_progress_bar(const char * title, ...)
{
  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   1,
    .font_size          =  10,
    .indent             =   0,
    .margin_left        =  10,
    .margin_right       =  10,
    .margin_top         =  30, // 70,
    .margin_bottom      =  10,
    .screen_left        = (Screen::WIDTH  -   WIDTH) >> 1,
    .screen_right       = (Screen::WIDTH  -   WIDTH) >> 1,
    .screen_top         = (Screen::HEIGHT -     150) >> 1,
    .screen_bottom      = (Screen::HEIGHT -     150) >> 1,
    .width              = 0,
    .height             = 0,
    .trim               = true,
    .font_style         = Fonts::NORMAL,
    .align              = CSS::CENTER_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM
  };

  char buff[80];

  va_list args;
  va_start(args, title);
  vsnprintf(buff, 80, title, args);
  va_end(args);

  page.start(fmt);

  page.clear_region(
    WIDTH, 
    150, 
    (Screen::WIDTH  - WIDTH ) >> 1, 
    (Screen::HEIGHT - 150) >> 1);

  page.put_highlight(
    WIDTH  - 4, 
    150    - 4, 
    ((Screen::WIDTH  - WIDTH ) >> 1) + 2,
    ((Screen::HEIGHT - 150) >> 1) + 2);

  // Title

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  std::string buffer = buff;
  page.put_text(buffer, fmt);
  page.end_paragraph(fmt);

  // Progress bar

  page.put_highlight(
    WIDTH - 40,
    30,
    ((Screen::WIDTH  - WIDTH ) >> 1) + 20,
    (Screen::HEIGHT >> 1) + 10
  );

  page.paint(false);
}

void MsgViewer::set_progress_bar(uint8_t value)
{
  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   1,
    .font_size          =  10,
    .indent             =   0,
    .margin_left        =  10,
    .margin_right       =  10,
    .margin_top         =  30, // 70,
    .margin_bottom      =  10,
    .screen_left        = (Screen::WIDTH  -   WIDTH ) >> 1,
    .screen_right       = (Screen::WIDTH  -   WIDTH ) >> 1, // Screen::WIDTH  - ((Screen::WIDTH  - WIDTH ) >> 1),
    .screen_top         = (Screen::HEIGHT -   HEIGHT) >> 1,
    .screen_bottom      = (Screen::HEIGHT -   HEIGHT) >> 1, // Screen::HEIGHT - ((Screen::HEIGHT - HEIGHT) >> 1),
    .width              = 0,
    .height             = 0,
    .trim               = true,
    .font_style         = Fonts::NORMAL,
    .align              = CSS::CENTER_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM
  };

  page.start(fmt);

  int16_t size = ((int32_t)(WIDTH - 46)) * value / 100;
  page.set_region(
    size,
    24,
    ((Screen::WIDTH - WIDTH ) >> 1) + 23,
    (Screen::HEIGHT >> 1) + 13
  );

  page.paint(false, true);
}