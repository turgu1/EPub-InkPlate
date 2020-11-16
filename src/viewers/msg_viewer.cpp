// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "viewers/msg_viewer.hpp"

#include "viewers/page.hpp"
#include "screen.hpp"

#include <cstdarg>

char MsgViewer::icon_char[3] = {  'I',  '!',  'H' };

void MsgViewer::show(Severity severity, const char * fmt_str, ...)
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
      .margin_left        =  50,
      .margin_right       =  10,
      .margin_top         =  50,
      .margin_bottom      =  10,
      .screen_left        = (Screen::WIDTH  - WIDTH ) >> 1,
      .screen_right       = Screen::WIDTH - (Screen::WIDTH  - WIDTH ) >> 1,
      .screen_top         = (Screen::HEIGHT - HEIGHT) >> 1,
      .screen_bottom      = Screen::HEIGHT - (Screen::HEIGHT - HEIGHT) >> 1,
      .width              = 0,
      .height             = 0,
      .trim               = true,
      .font_style         = Fonts::NORMAL,
      .align              = CSS::LEFT_ALIGN,
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
  fmt.font_size  = 12;

  std::string buffer = buff;
  page.put_text(buffer, fmt);

  page.paint(false);
}