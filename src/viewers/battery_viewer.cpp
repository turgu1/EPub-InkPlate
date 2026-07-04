// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "viewers/battery_viewer.hpp"

#if EPUB_INKPLATE_BUILD
  #include "battery.hpp"
  #include "config.hpp"
  #include "logging.hpp"
  #include "screen.hpp"

  #include <cstring>

  auto BatteryViewer::show(PagePtr &page) -> void {
    int8_t viewMode = 0;
    config.get(Config::Ident::BATTERY, &viewMode);

    if (viewMode == 0) { return; }

    float voltage = battery.read_level();

    LOG_D("Battery voltage: {:5.3f}", voltage);

    Page::Format fmt = {
      .fontSize = 9,
    };

    // Show battery icon

    FontPtr &font = appFonts.getFont(0);

    float    value       = ((voltage - 2.5) * 4.0) / 1.2;
    int16_t  iconIndex = value; // max is 3.7
    if (iconIndex > 4) { iconIndex = 4; }

    static constexpr char icons[5] = { '0', '1', '2', '3', '4' };

    Glyph *               glyph = font->getGlyph(icons[iconIndex], 9);

    Dim                   dim;
    dim.width  = 100;
    dim.height = -font->getDescenderHeight(9);

    Pos pos;
    pos.y = Screen::getHeight() + font->getDescenderHeight(9) - 2;

    // page->clearRegion(dim, pos);

    fmt.fontIndex = ICONS_FONT_INDEX;

    #if BLE_KEYPAD
      // Leave space for the Bluetooth pairing state icon
      pos.x         = 25;
    #else
      pos.x         = 10;
    #endif

    page->putCharAt(icons[iconIndex], pos, fmt);

    // LOG_E("Battery icon index: {} ({})", iconIndex, icons[iconIndex]);

    // Show text

    if ((viewMode == 1) || (viewMode == 2)) {
      char str[15];

      if (viewMode == 1) {
        int percentage = ((voltage - 2.5) * 100.0) / 1.2;
        if (percentage > 100) { percentage = 100; }
        sprintf(str, "%d%c", percentage, '%');
      } else if (viewMode == 2) {
        sprintf(str, "%5.2fv", voltage);
      }

      fmt.fontIndex = SYSTEM_REGULAR_FONT_INDEX;
      pos.x         += (glyph != nullptr ? glyph->advance : 10) + 5;
      page->putStrAt(str, pos, fmt);
    }
  }
#endif