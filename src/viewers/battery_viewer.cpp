#define __BATTERY_VIEWER__ 1
#include "viewers/battery_viewer.hpp"

#if EPUB_INKPLATE6_BUILD
  #include "viewers/page.hpp"
  #include "models/config.hpp"
  #include "inkplate6_ctrl.hpp"
  #include "screen.hpp"
  #include "logging.hpp"

  #include <cstring>

  static constexpr char const * TAG = "BatteryViewer";

  void
  BatteryViewer::show()
  {
    int8_t view_mode;
    config.get(Config::BATTERY, &view_mode);

    if (view_mode == 0) return;

    float voltage = inkplate6_ctrl.read_battery();

    LOG_D("Battery voltage: %5.3f", voltage);

    Page::Format fmt = {
      .line_height_factor = 1.0,
      .font_index         = 1,
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
      .trim               = true,
      .font_style         = Fonts::NORMAL,
      .align              = CSS::LEFT_ALIGN,
      .text_transform     = CSS::NO_TRANSFORM
    };

    // Show battery icon

    TTF * font = fonts.get(0, 9);
    float   value = ((voltage - 2.5) * 4.0) / 1.2;
    int16_t icon_index =  value; // max is 3.7
    if (icon_index > 4) icon_index = 4;

    static constexpr char icons[5] = { '0', '1', '2', '3', '4' };

    TTF::BitmapGlyph * glyph = font->get_glyph(icons[icon_index]);

    page.clear_region(100, -font->get_descender_height(), 4, Screen::HEIGHT + font->get_descender_height() - 2);

    fmt.font_index = 0;  
    page.put_char_at(icons[icon_index], 5, Screen::HEIGHT + font->get_descender_height() - 2, fmt);

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

      font = fonts.get(1, 9);
      fmt.font_index = 1;  
      page.put_str_at(str, 5 + glyph->advance + 5, Screen::HEIGHT + font->get_descender_height() - 2, fmt);
    }
  }
#endif