// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#if 0

// Not ready yet

#define __KEYBOARD_VIEWER__ 1
#include "viewers/keyboard_viewer.hpp"

#if EPUB_INKPLATE_BUILD
  #include "nvs.h"
  #include "inkplate_platform.hpp"
#endif

bool 
KeyboardViewer::get_alfanum(char * str, uint16_t len, UpdateHandler handler)
{
  width           = Screen::WIDTH - 60;
  current_kb_type = KBType::ALFA;

  if (page.get_compute_mode() == Page::ComputeMode::LOCATION) return false; // Cannot be used durint location computation

  fmt = {
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

#if 0
  page.clear_region(
    Dim(width, HEIGHT), 
    Pos((Screen::WIDTH  - width ) >> 1, (Screen::HEIGHT - HEIGHT) >> 1));

  page.put_highlight(
    Dim(width - 4, HEIGHT - 4), 
    Pos(((Screen::WIDTH - width ) >> 1) + 2, ((Screen::HEIGHT - HEIGHT) >> 1) + 2));

  TTF * font = fonts.get(0);
  Font::Glyph * glyph = font->get_glyph(icon_char[severity], 24);

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
#endif

  page.paint(false);

  return true;
}

void
KeyboardViewer::show_kb(KBType kb_type)
{
  uint16_t x, y;

  switch (kb_type) {
    case KBType::ALFA:
      show_line(alfa_line_1_low, x, y);
      show_line(alfa_line_2_low, x, y);
      show_line(alfa_line_3_low, x, y);
      show_line(alfa_line_4, x, y);
      break;

    case KBType::ALFA_SHIFTED:
      show_line(alfa_line_1_upp, x, y);
      show_line(alfa_line_2_upp, x, y);
      show_line(alfa_line_3_upp, x, y);
      show_line(alfa_line_4, x, y);
      break;
    
    case KBType::NUMBERS:
      show_line(num_line_1, x, y);
      show_line(num_line_2, x, y);
      show_line(num_line_3, x, y);
      show_line(num_line_4, x, y);
      break;
      
    case KBType::SPECIAL:
      show_line(special_line_1, x, y);
      show_line(special_line_2, x, y);
      show_line(special_line_3, x, y);
      show_line(special_line_4, x, y);
      break;
  }
}

void 
KeyboardViewer::show_line(const char * line, uint16_t & x, uint16_t y)
{

}

void 
KeyboardViewer::show_char(char ch, uint16_t & x, uint16_t y)
{

}

#endif