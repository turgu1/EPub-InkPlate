// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BATTERY_VIEWER__ 1
#include "viewers/battery_viewer.hpp"

#if EPUB_INKPLATE_BUILD
  #include "viewers/page.hpp"
  #include "models/config.hpp"
  #include "battery.hpp"
  #include "screen.hpp"
  #include "logging.hpp"

  #include <cstring>

  static constexpr char const * TAG = "BatteryViewer";

  void
  BatteryViewer::show()
  {
    int8_t view_mode;
    config.get(Config::Ident::BATTERY, &view_mode);

    if (view_mode == 0) return;

    float voltage = battery.read_level();

    LOG_D("Battery voltage: %5.3f", voltage);

    Page::Format fmt = {
      .line_height_factor = 1.0,
      .font_index         = 5,
      .font_size          = 9,
      .indent             = 0,
      .margin_left        = 0,
      .margin_right       = 0,
      .margin_top         = 0,
      .margin_bottom      = 0,
      .screen_left        = 10,
      .screen_right       = 10,
      .screen_top         = 10,
      .screen_bottom      = 10,
      .width              = 0,
      .height             = 0,
      .vertical_align     = 0,
      .trim               = true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

    // Show battery icon

    Font * font = fonts.get(0);
    float   value = ((voltage - 2.5) * 4.0) / 1.2;
    int16_t icon_index =  value; // max is 3.7
    if (icon_index > 4) icon_index = 4;

    static constexpr char icons[5] = { '0', '1', '2', '3', '4' };

    Font::Glyph * glyph = font->get_glyph(icons[icon_index], 9);

    Dim dim;
    dim.width  =  100;
    dim.height = -font->get_descender_height(9);

    Pos pos;
    pos.x = 4;
    pos.y = Screen::HEIGHT + font->get_descender_height(9) - 2;

    page.clear_region(dim, pos);

    fmt.font_index = 0;  
    pos.x          = 5;
    page.put_char_at(icons[icon_index], pos, fmt);

    // LOG_E("Battery icon index: %d (%c)", icon_index, icons[icon_index]);

    // Show text

    if ((view_mode == 1) || (view_mode == 2)) {
      char str[10];

      if (view_mode == 1) {
        int percentage = ((voltage - 2.5) * 100.0) / 1.2;
        if (percentage > 100) percentage = 100;
        sprintf(str, "%d%c", percentage, '%');
      }
      else if (view_mode == 2) {
        sprintf(str, "%5.2fv", voltage);
      }

      font = fonts.get(5);
      fmt.font_index = 5;  
      pos.x = 5 + glyph->advance + 5;
      page.put_str_at(str, pos, fmt);
    }
  }
#endif