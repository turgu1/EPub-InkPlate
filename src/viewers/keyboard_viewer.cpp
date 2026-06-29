// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#if 0

// Not ready yet

  #define __KEYBOARD_VIEWER__ 1
  #include "viewers/keyboard_viewer.hpp"

  #if EPUB_INKPLATE_BUILD
    #include "inkplate_platform.hpp"
    #include "nvs.h"
  #endif

  auto KeyboardViewer::getAlfanum(char * str, uint16_t len, UpdateHandler handler) -> bool
  {
    width           = Screen::getWidth() - 60;
    current_kb_type = KBType::ALFA;

    if (page.getComputeMode() == Page::ComputeMode::LOCATION) { return false; } // Cannot be used durint location computation

    fmt = {
      .fontIndex         =  ICONS_FONT_INDEX,
      .fontSize          =  24,
      .marginTop         =  30,
      .screenLeft        =  (Screen::getWidth()  - width ) >> 1,
      .screenRight       =  (Screen::getWidth()  - width ) >> 1,
      .screenTop         =  (Screen::getHeight() - HEIGHT) >> 1,
      .screenBottom      =  (Screen::getHeight() - HEIGHT) >> 1,
      .align              =  HAlign::CENTER,
    };

    page.setComputeMode(Page::ComputeMode::DISPLAY);

    page.start(fmt);

    #if 0
      page.clearRegion(
        Dim(width, HEIGHT),
        Pos((Screen::getWidth()  - width ) >> 1, (Screen::getHeight() - HEIGHT) >> 1));

      page.putHighlight(
        Dim(width - 4, HEIGHT - 4),
        Pos(((Screen::getWidth() - width ) >> 1) + 2, ((Screen::getHeight() - HEIGHT) >> 1) + 2));

      FontPtr &font = appFonts.getFont(0);

      Glyph *  glyph = font->getGlyph(icon_char[severity], 24);

      if (glyph != nullptr) {
        page.putCharAt(
          icon_char[severity],
          Pos(((Screen::getWidth()  - width ) >> 1) + 50 - (glyph->dim.width >> 1), ( Screen::getHeight() >> 1) + 20),
          fmt);
      }

      fmt.fontIndex =  SYSTEM_REGULAR_FONT_INDEX;
      fmt.fontSize  = 10;

      // Title

      page.setLimits(fmt);
      page.newParagraph(fmt);
      std::string buffer = title;
      page.addText(buffer, fmt);
      page.endParagraph(fmt);

      // Message

      fmt.align       = HAlign::LEFT;
      fmt.marginTop  = 80;
      fmt.marginLeft = 90;

      page.setLimits(fmt);
      page.newParagraph(fmt);
      buffer = buff;
      page.addText(buffer, fmt);
      page.endParagraph(fmt);

      // Press a Key option

      if (press_a_key) {
        fmt.align = HAlign::CENTER;
        fmt.fontSize   =   9;
        fmt.marginLeft =  10;
        fmt.marginTop  = 200;

        page.setLimits(fmt);
        page.newParagraph(fmt);
        buffer = "[Press a key]";
        page.addText(buffer, fmt);
        page.endParagraph(fmt);
      }
    #endif

    page.paint(false);

    return true;
  }

  void
  KeyboardViewer::showKb(KBType kbType)
  {
    uint16_t x, y;

    switch (kbType) {
    case KBType::ALFA:
      showLine(alfa_line_1_low, x, y);
      showLine(alfa_line_2_low, x, y);
      showLine(alfa_line_3_low, x, y);
      showLine(alfa_line_4,     x, y);
      break;

    case KBType::ALFA_SHIFTED:
      showLine(alfa_line_1_upp, x, y);
      showLine(alfa_line_2_upp, x, y);
      showLine(alfa_line_3_upp, x, y);
      showLine(alfa_line_4,     x, y);
      break;

    case KBType::NUMBERS:
      showLine(num_line_1, x, y);
      showLine(num_line_2, x, y);
      showLine(num_line_3, x, y);
      showLine(num_line_4, x, y);
      break;

    case KBType::SPECIAL:
      showLine(special_line_1, x, y);
      showLine(special_line_2, x, y);
      showLine(special_line_3, x, y);
      showLine(special_line_4, x, y);
      break;
    }
  }

  void
  KeyboardViewer::showLine(const char * line, uint16_t & x, uint16_t y)
  {

  }

  void
  KeyboardViewer::showChar(char ch, uint16_t & x, uint16_t y)
  {

  }

#endif