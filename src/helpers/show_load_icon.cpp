// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "helpers/show_load_icon.hpp"

#include "fonts.hpp"
#include "screen.hpp"

auto showLoadIcon(const Dim &imageDim) -> void {
  if ((imageDim.width <= 200) && (imageDim.height <= 200)) return;

  FontPtr &iconFont = appFonts.getFont(0);
  Glyph *glyph      = iconFont->getGlyph('N', 24);
  if ((glyph == nullptr) || (glyph->buffer == nullptr) || (glyph->dim.width == 0) ||
      (glyph->dim.height == 0)) {
    return;
  }

  int16_t iconX = (Screen::getWidth() - glyph->dim.width) / 2;
  int16_t iconY = (Screen::getHeight() - (Screen::getHeight() / 4)) - (glyph->dim.height / 2);

  int16_t rectW = glyph->dim.width * 2;
  int16_t rectH = glyph->dim.height * 2;
  int16_t rectX = iconX - (glyph->dim.width / 2);
  int16_t rectY = iconY - (glyph->dim.height / 2);

  int16_t clearW = rectW + 20;
  int16_t clearH = rectH + 20;
  int16_t clearX = rectX - 10;
  int16_t clearY = rectY - 10;

  if (rectX < 0) rectX = 0;
  if (rectY < 0) rectY = 0;
  if (rectX + rectW > Screen::getWidth()) rectW = Screen::getWidth() - rectX;
  if (rectY + rectH > Screen::getHeight()) rectH = Screen::getHeight() - rectY;

  if (clearX < 0) clearX = 0;
  if (clearY < 0) clearY = 0;
  if (clearX + clearW > Screen::getWidth()) clearW = Screen::getWidth() - clearX;
  if (clearY + clearH > Screen::getHeight()) clearH = Screen::getHeight() - clearY;

  screen.colorizeRegion(Dim(static_cast<uint16_t>(clearW), static_cast<uint16_t>(clearH)),
                        Pos(static_cast<uint16_t>(clearX), static_cast<uint16_t>(clearY)),
                        Screen::Color::WHITE);

  screen.drawRoundRectangle(Dim(static_cast<uint16_t>(rectW), static_cast<uint16_t>(rectH)),
                            Pos(static_cast<uint16_t>(rectX), static_cast<uint16_t>(rectY)),
                            Screen::Color::BLACK);
  screen.drawGlyph(glyph->buffer, glyph->dim,
                   Pos(static_cast<uint16_t>(iconX), static_cast<uint16_t>(iconY)), glyph->pitch);
  screen.update(true);
}
