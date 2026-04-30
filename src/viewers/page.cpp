// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "picture.hpp"

#include "alloc.hpp"
#include "screen.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/page.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

// clang-format off
Page::Entities Page::entities 
  = {
    {"nbsp",   0x00A0}, // no-break space = non-breaking space, U+00A0 ISOnum
    {"iexcl",  0x00A1}, // inverted exclamation mark, U+00A1 ISOnum
    {"cent",   0x00A2}, // cent sign, U+00A2 ISOnum
    {"pound",  0x00A3}, // pound sign, U+00A3 ISOnum
    {"curren", 0x00A4}, // currency sign, U+00A4 ISOnum
    {"yen",    0x00A5}, // yen sign = yuan sign, U+00A5 ISOnum
    {"brvbar", 0x00A6}, // broken bar = broken vertical bar, U+00A6 ISOnum
    {"sect",   0x00A7}, // section sign, U+00A7 ISOnum
    {"uml",    0x00A8}, // diaeresis = spacing diaeresis, U+00A8 ISOdia
    {"copy",   0x00A9}, // copyright sign, U+00A9 ISOnum
    {"ordf",   0x00AA}, // feminine ordinal indicator, U+00AA ISOnum
    {"laquo",  0x00AB}, // left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum
    {"not",    0x00AC}, // not sign = angled dash, U+00AC ISOnum
    {"shy",    0x00AD}, // soft hyphen = discretionary hyphen, U+00AD ISOnum
    {"reg",    0x00AE}, // registered sign = registered trade mark sign, U+00AE ISOnum
    {"macr",   0x00AF}, // macron = spacing macron = overline = APL overbar, U+00AF ISOdia
    {"deg",    0x00B0}, // degree sign, U+00B0 ISOnum
    {"plusmn", 0x00B1}, // plus-minus sign = plus-or-minus sign, U+00B1 ISOnum
    {"sup2",   0x00B2}, // superscript two = superscript digit two = squared, U+00B2 ISOnum
    {"sup3",   0x00B3}, // superscript three = superscript digit three = cubed, U+00B3 ISOnum
    {"acute",  0x00B4}, // acute accent = spacing acute, U+00B4 ISOdia
    {"micro",  0x00B5}, // micro sign, U+00B5 ISOnum
    {"para",   0x00B6}, // pilcrow sign = paragraph sign, U+00B6 ISOnum
    {"middot", 0x00B7}, // middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum
    {"cedil",  0x00B8}, // cedilla = spacing cedilla, U+00B8 ISOdia
    {"sup1",   0x00B9}, // superscript one = superscript digit one, U+00B9 ISOnum
    {"ordm",   0x00BA}, // masculine ordinal indicator, U+00BA ISOnum
    {"raquo",  0x00BB}, // right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum
    {"frac14", 0x00BC}, // vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum
    {"frac12", 0x00BD}, // vulgar fraction one half = fraction one half, U+00BD ISOnum
    {"frac34", 0x00BE}, // vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum
    {"iquest", 0x00BF}, // inverted question mark = turned question mark, U+00BF ISOnum
    {"Agrave", 0x00C0}, // latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1
    {"Aacute", 0x00C1}, // latin capital letter A with acute, U+00C1 ISOlat1
    {"Acirc",  0x00C2}, // latin capital letter A with circumflex, U+00C2 ISOlat1
    {"Atilde", 0x00C3}, // latin capital letter A with tilde, U+00C3 ISOlat1
    {"Auml",   0x00C4}, // latin capital letter A with diaeresis, U+00C4 ISOlat1
    {"Aring",  0x00C5}, // latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1
    {"AElig",  0x00C6}, // latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1
    {"Ccedil", 0x00C7}, // latin capital letter C with cedilla, U+00C7 ISOlat1
    {"Egrave", 0x00C8}, // latin capital letter E with grave, U+00C8 ISOlat1
    {"Eacute", 0x00C9}, // latin capital letter E with acute, U+00C9 ISOlat1
    {"Ecirc",  0x00CA}, // latin capital letter E with circumflex, U+00CA ISOlat1
    {"Euml",   0x00CB}, // latin capital letter E with diaeresis, U+00CB ISOlat1
    {"Igrave", 0x00CC}, // latin capital letter I with grave, U+00CC ISOlat1
    {"Iacute", 0x00CD}, // latin capital letter I with acute, U+00CD ISOlat1
    {"Icirc",  0x00CE}, // latin capital letter I with circumflex, U+00CE ISOlat1
    {"Iuml",   0x00CF}, // latin capital letter I with diaeresis, U+00CF ISOlat1
    {"ETH",    0x00D0}, // latin capital letter ETH, U+00D0 ISOlat1
    {"Ntilde", 0x00D1}, // latin capital letter N with tilde, U+00D1 ISOlat1
    {"Ograve", 0x00D2}, // latin capital letter O with grave, U+00D2 ISOlat1
    {"Oacute", 0x00D3}, // latin capital letter O with acute, U+00D3 ISOlat1
    {"Ocirc",  0x00D4}, // latin capital letter O with circumflex, U+00D4 ISOlat1
    {"Otilde", 0x00D5}, // latin capital letter O with tilde, U+00D5 ISOlat1
    {"Ouml",   0x00D6}, // latin capital letter O with diaeresis, U+00D6 ISOlat1
    {"times",  0x00D7}, // multiplication sign, U+00D7 ISOnum
    {"Oslash", 0x00D8}, // latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1
    {"Ugrave", 0x00D9}, // latin capital letter U with grave, U+00D9 ISOlat1
    {"Uacute", 0x00DA}, // latin capital letter U with acute, U+00DA ISOlat1
    {"Ucirc",  0x00DB}, // latin capital letter U with circumflex, U+00DB ISOlat1
    {"Uuml",   0x00DC}, // latin capital letter U with diaeresis, U+00DC ISOlat1
    {"Yacute", 0x00DD}, // latin capital letter Y with acute, U+00DD ISOlat1
    {"THORN",  0x00DE}, // latin capital letter THORN, U+00DE ISOlat1
    {"szlig",  0x00DF}, // latin small letter sharp s = ess-zed, U+00DF ISOlat1
    {"agrave", 0x00E0}, // latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1
    {"aacute", 0x00E1}, // latin small letter a with acute, U+00E1 ISOlat1
    {"acirc",  0x00E2}, // latin small letter a with circumflex, U+00E2 ISOlat1
    {"atilde", 0x00E3}, // latin small letter a with tilde, U+00E3 ISOlat1
    {"auml",   0x00E4}, // latin small letter a with diaeresis, U+00E4 ISOlat1
    {"aring",  0x00E5}, // latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1
    {"aelig",  0x00E6}, // latin small letter ae = latin small ligature ae, U+00E6 ISOlat1
    {"ccedil", 0x00E7}, // latin small letter c with cedilla, U+00E7 ISOlat1
    {"egrave", 0x00E8}, // latin small letter e with grave, U+00E8 ISOlat1
    {"eacute", 0x00E9}, // latin small letter e with acute, U+00E9 ISOlat1
    {"ecirc",  0x00EA}, // latin small letter e with circumflex, U+00EA ISOlat1
    {"euml",   0x00EB}, // latin small letter e with diaeresis, U+00EB ISOlat1
    {"igrave", 0x00EC}, // latin small letter i with grave, U+00EC ISOlat1
    {"iacute", 0x00ED}, // latin small letter i with acute, U+00ED ISOlat1
    {"icirc",  0x00EE}, // latin small letter i with circumflex, U+00EE ISOlat1
    {"iuml",   0x00EF}, // latin small letter i with diaeresis, U+00EF ISOlat1
    {"eth",    0x00F0}, // latin small letter eth, U+00F0 ISOlat1
    {"ntilde", 0x00F1}, // latin small letter n with tilde, U+00F1 ISOlat1
    {"ograve", 0x00F2}, // latin small letter o with grave, U+00F2 ISOlat1
    {"oacute", 0x00F3}, // latin small letter o with acute, U+00F3 ISOlat1
    {"ocirc",  0x00F4}, // latin small letter o with circumflex, U+00F4 ISOlat1
    {"otilde", 0x00F5}, // latin small letter o with tilde, U+00F5 ISOlat1
    {"ouml",   0x00F6}, // latin small letter o with diaeresis, U+00F6 ISOlat1
    {"divide", 0x00F7}, // division sign, U+00F7 ISOnum
    {"oslash", 0x00F8}, // latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1
    {"ugrave", 0x00F9}, // latin small letter u with grave, U+00F9 ISOlat1
    {"uacute", 0x00FA}, // latin small letter u with acute, U+00FA ISOlat1
    {"ucirc",  0x00FB}, // latin small letter u with circumflex, U+00FB ISOlat1
    {"uuml",   0x00FC}, // latin small letter u with diaeresis, U+00FC ISOlat1
    {"yacute", 0x00FD}, // latin small letter y with acute, U+00FD ISOlat1
    {"thorn",  0x00FE}, // latin small letter thorn, U+00FE ISOlat1
    {"yuml",   0x00FF}, // latin small letter y with diaeresis, U+00FF ISOlat1
    {"quot",   0x0022}, // quotation mark, U+0022 ISOnum
    {"amp",    0x0026}, // ampersand, U+0026 ISOnum
    {"lt",     0x003C}, // less-than sign, U+003C ISOnum
    {"gt",     0x003E}, // greater-than sign, U+003E ISOnum
    {"apos",   0x0027}, // apostrophe = APL quote, U+0027 ISOnum
    {"OElig",  0x0152}, // latin capital ligature OE, U+0152 ISOlat2
    {"oelig",  0x0153}, // latin small ligature oe, U+0153 ISOlat2
    {"Scaron", 0x0160}, // latin capital letter S with caron, U+0160 ISOlat2
    {"scaron", 0x0161}, // latin small letter s with caron, U+0161 ISOlat2
    {"Yuml",   0x0178}, // latin capital letter Y with diaeresis, U+0178 ISOlat2
    {"circ",   0x02C6}, // modifier letter circumflex accent, U+02C6 ISOpub
    {"tilde",  0x02DC}, // small tilde, U+02DC ISOdia
    {"ensp",   0x2002}, // en space, U+2002 ISOpub
    {"emsp",   0x2003}, // em space, U+2003 ISOpub
    {"thinsp", 0x2009}, // thin space, U+2009 ISOpub
    {"zwnj",   0x200C}, // zero width non-joiner, U+200C NEW RFC 2070
    {"zwj",    0x200D}, // zero width joiner, U+200D NEW RFC 2070
    {"lrm",    0x200E}, // left-to-right mark, U+200E NEW RFC 2070
    {"rlm",    0x200F}, // right-to-left mark, U+200F NEW RFC 2070
    {"ndash",  0x2013}, // en dash, U+2013 ISOpub
    {"mdash",  0x2014}, // em dash, U+2014 ISOpub
    {"lsquo",  0x2018}, // left single quotation mark, U+2018 ISOnum
    {"rsquo",  0x2019}, // right single quotation mark, U+2019 ISOnum
    {"sbquo",  0x201A}, // single low-9 quotation mark, U+201A NEW
    {"ldquo",  0x201C}, // left double quotation mark,  U+201C ISOnum
    {"rdquo",  0x201D}, // right double quotation mark, U+201D ISOnum
    {"bdquo",  0x201E}, // double low-9 quotation mark, U+201E NEW
    {"dagger", 0x2020}, // dagger, U+2020 ISOpub
    {"Dagger", 0x2021}, // double dagger, U+2021 ISOpub
    {"permil", 0x2030}, // per mille sign, U+2030 ISOtech
    {"lsaquo", 0x2039}, // single left-pointing angle quotation mark, U+2039 ISO proposed
    {"rsaquo", 0x203A}, // single right-pointing angle quotation mark, U+203A ISO proposed
    {"euro",   0x20AC}, // euro sign, U+20AC NEW
};
// clang-format on

auto Page::clean() -> void {
  displayList->clear();
  lineList->clear();
  paraIndent = 0;
  topMargin  = 0;
  // computeMode   = ComputeMode::DISPLAY;
  screenIsFull = false;
}

// 00000000 -- 0000007F: 	0xxxxxxx
// 00000080 -- 000007FF: 	110xxxxx 10xxxxxx
// 00000800 -- 0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
// 00010000 -- 001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

auto Page::toUnicode(const char *str, CSS::TextTransform transform, bool first,
                     const char **str2) const -> int32_t {
  const uint8_t *c = reinterpret_cast<const uint8_t *>(str);
  int32_t u        = ' ';
  bool done        = false;

  if (*c == '&') {
    const uint8_t *name = c + 1;
    const uint8_t *end  = name;
    uint8_t len         = 0;
    while ((len < 7) && (*end != 0) && (*end != ';')) {
      ++end;
      ++len;
    }

    if (*end == ';') {
      LOG_I("Entity: %.*s", len, name);
      auto theStr = HimemString((char *)(name), (size_t)(end - name));
      auto search = entities.find(theStr);
      if (search != entities.end()) {
        u = search->second;
      }
    }

    if (u == ' ') {
      // Unknown or malformed entity: consume '&' and return it as-is.
      u = '&';
      ++c;
    } else {
      c = end + 1;
    }
    done = true;
  } else if ((*c & 0x80) == 0x00) {
    u    = *c++;
    done = true;
  } else if (((*c & 0xF8) == 0xF0) && (c[1] != 0) && (c[2] != 0) && (c[3] != 0) &&
             ((c[1] & 0xC0) == 0x80) && ((c[2] & 0xC0) == 0x80) && ((c[3] & 0xC0) == 0x80)) {
    u = (c[0] & 0x07);
    u = (u << 6) + (c[1] & 0x3F);
    u = (u << 6) + (c[2] & 0x3F);
    u = (u << 6) + (c[3] & 0x3F);
    c += 4;
    done = true;
  } else if (((*c & 0xF0) == 0xE0) && (c[1] != 0) && (c[2] != 0) && ((c[1] & 0xC0) == 0x80) &&
             ((c[2] & 0xC0) == 0x80)) {
    u = (c[0] & 0x0F);
    u = (u << 6) + (c[1] & 0x3F);
    u = (u << 6) + (c[2] & 0x3F);
    c += 3;
    done = true;
  } else if (((*c & 0xE0) == 0xC0) && (c[1] != 0) && ((c[1] & 0xC0) == 0x80)) {
    u = (c[0] & 0x1F);
    u = (u << 6) + (c[1] & 0x3F);
    c += 2;
    done = true;
  } else {
    // Invalid leading byte or malformed UTF-8 sequence; consume one byte to avoid stalling.
    ++c;
  }

  *str2 = reinterpret_cast<const char *>(c);

  if (done && (u >= 0) && (u <= 0x7F) && (transform != CSS::TextTransform::NONE)) {
    if (transform == CSS::TextTransform::UPPERCASE)
      u = toupper(static_cast<unsigned char>(u));
    else if (transform == CSS::TextTransform::LOWERCASE)
      u = tolower(static_cast<unsigned char>(u));
    else if (first && (transform == CSS::TextTransform::CAPITALIZE))
      u = toupper(static_cast<unsigned char>(u));
  }

  return done ? u : ' ';
}

auto Page::putStrAt(const std::string &str, Pos pos, const Format &fmt) -> void {
  Glyph *glyph;

  FontPtr &font = fonts.getFont(fmt.fontIndex);

  const char *s = str.c_str();
  if (fmt.align == CSS::Align::LEFT) {
    bool first = true;
    while (*s) {
      const char *s1;
      glyph = font->getGlyph(toUnicode(s, fmt.textTransform, first, &s1), fmt.fontSize);
      s     = s1;
      if (glyph != nullptr) {
        DisplayListEntry *entry = displayList->getNewEntry();
        if (entry == nullptr) return;
        entry->command = DisplayListCommand::GLYPH;
        entry->pos     = {static_cast<uint16_t>(pos.x + glyph->xoff),
                          static_cast<uint16_t>(pos.y + glyph->yoff)};
        entry->v       = GlyphEntry{glyph, glyph->advance, false};

        #if DEBUGGING
          // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
          //   LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
          // }
          // else
          if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
            LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
          }
        #endif

        displayList->pushBack(entry);

        pos.x += glyph->advance;
      }
      first = false;
    }
  } else {
    int16_t size  = 0;
    const char *s = str.c_str();

    while (*s) {
      bool first = true;
      const char *s1;
      glyph = font->getGlyph(toUnicode(s, fmt.textTransform, first, &s1), fmt.fontSize);
      s     = s1;
      if (glyph != nullptr) size += glyph->advance;
      first = false;
    }

    int16_t x;

    if (pos.x == HORIZONTAL_CENTER) {
      if (fmt.align == CSS::Align::CENTER) {
        x = minX + ((maxX - minX) >> 1) - (size >> 1);
      } else { // RIGHT
        x = maxX - size;
      }
    } else {
      if (fmt.align == CSS::Align::CENTER) {
        x = pos.x - (size >> 1);
      } else { // RIGHT
        x = pos.x - size;
      }
    }

    s          = str.c_str();
    bool first = true;
    while (*s) {
      const char *s1;
      glyph = font->getGlyph(toUnicode(s, fmt.textTransform, first, &s1), fmt.fontSize);
      s     = s1;
      if (glyph != nullptr) {

        DisplayListEntry *entry = displayList->getNewEntry();
        if (entry == nullptr) return;

        entry->command = DisplayListCommand::GLYPH;
        entry->pos     = {static_cast<uint16_t>(x + glyph->xoff),
                          static_cast<uint16_t>(pos.y + glyph->yoff)};
        entry->v       = GlyphEntry{glyph, glyph->advance, false};

        #if DEBUGGING
          // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
          //   LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
          // }
          // else
          if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
            LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
          }
        #endif

        displayList->pushBack(entry);

        x += glyph->advance;
      }
      first = false;
    }
  }
}

auto Page::putCharAt(char ch, Pos pos, const Format &fmt) -> void {
  Glyph *glyph;

  FontPtr &font = fonts.getFont(fmt.fontIndex);

  glyph = font->getGlyph(ch, fmt.fontSize);
  if (glyph != nullptr) {
    DisplayListEntry *entry = displayList->getNewEntry();
    if (entry == nullptr) return;
    entry->command = DisplayListCommand::GLYPH;
    entry->pos     = {static_cast<uint16_t>(pos.x + glyph->xoff),
                      static_cast<uint16_t>(pos.y + glyph->yoff)};
    entry->v       = GlyphEntry{glyph, glyph->advance, false};

    #if DEBUGGING
      // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
      //   LOG_E("Put_char_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
      // }
      // else
      if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
        LOG_E("Put_char_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
      }
    #endif

    displayList->pushBack(entry);
  }
}

auto Page::paint(bool clearScreen, bool noFull, bool doIt) -> void {
  if (!doIt)
    if ((displayList->empty()) || (computeMode != ComputeMode::DISPLAY)) return;

  // displayList->show("DISPLAY LIST");

  if (clearScreen) screen.clear();

  // int count = 0;

  for (auto *entry : *displayList) {
    switch (entry->command) {
    case DisplayListCommand::GLYPH:
      if (std::holds_alternative<GlyphEntry>(entry->v) &&
          std::get<GlyphEntry>(entry->v).glyph != nullptr) {
        screen.drawGlyph(std::get<GlyphEntry>(entry->v).glyph->buffer,
                         std::get<GlyphEntry>(entry->v).glyph->dim, entry->pos,
                         std::get<GlyphEntry>(entry->v).glyph->pitch);
      } else {
        LOG_E("DISPLAY LIST CORRUPTED!!");
      }
      break;
    case DisplayListCommand::PICTURE:
      screen.drawPicture(std::get<PictureEntry>(entry->v).picture, entry->pos);
      break;
    case DisplayListCommand::HIGHLIGHT:
      screen.drawRectangle(std::get<RegionEntry>(entry->v).dim, entry->pos, Screen::Color::BLACK);
      break;
    case DisplayListCommand::CLEAR_HIGHLIGHT:
      screen.drawRectangle(std::get<RegionEntry>(entry->v).dim, entry->pos, Screen::Color::WHITE);
      break;
    case DisplayListCommand::ROUNDED:
      screen.drawRoundRectangle(std::get<RegionEntry>(entry->v).dim, entry->pos,
                                Screen::Color::BLACK);
      break;
    case DisplayListCommand::CLEAR_ROUNDED:
      screen.drawRoundRectangle(std::get<RegionEntry>(entry->v).dim, entry->pos,
                                Screen::Color::WHITE);
      break;
    case DisplayListCommand::CLEAR_REGION:
      screen.colorizeRegion(std::get<RegionEntry>(entry->v).dim, entry->pos, Screen::Color::WHITE);
      break;
    case DisplayListCommand::SET_REGION:
      screen.colorizeRegion(std::get<RegionEntry>(entry->v).dim, entry->pos, Screen::Color::BLACK);
      break;
    }

    // count++;
  }

  // LOG_I("Painted %d entries on screen", count);

  screen.update(noFull);
}

auto Page::start(const Format &fmt) -> void {
  pos.x = fmt.screenLeft;
  pos.y = fmt.screenTop;

  minY = fmt.screenTop;
  maxX = Screen::getWidth() - fmt.screenRight;
  maxY = Screen::getHeight() - fmt.screenBottom;
  minX = fmt.screenLeft;

  paraMinX = minX;
  paraMaxX = maxX;

  screenIsFull = false;

  displayList->clear();
  lineList->clear();

  paraIndent   = 0;
  lineWidth    = 0;
  glyphsHeight = 0;
}

auto Page::setLimits(const Format &fmt) -> void {
  pos.x = pos.y = 0;

  minY = fmt.screenTop;
  maxX = Screen::getWidth() - fmt.screenRight;
  maxY = Screen::getHeight() - fmt.screenBottom;
  minX = fmt.screenLeft;

  screenIsFull = false;

  lineList->clear();

  paraIndent   = 0;
  lineWidth    = 0;
  topMargin    = 0;
  glyphsHeight = 0;
}

auto Page::lineBreak(const Format &fmt, int8_t indentNextLine) -> bool {
  FontPtr &font = fonts.getFont(fmt.fontIndex);

  if (!lineList->empty()) {
    addLine(fmt, false);
  } else {
    pos.y += font->getLineHeight(fmt.fontSize) * fmt.lineHeightFactor;
    pos.x = minX + indentNextLine;
  }

  screenIsFull = screenIsFull || ((pos.y - font->getDescenderHeight(fmt.fontSize)) >= maxY);
  if (screenIsFull) {
    return false;
  }
  return true;
}

auto Page::newParagraph(const Format &fmt, bool recover) -> bool {
  FontPtr &font = fonts.getFont(fmt.fontIndex);

  // Check if there is enough room for the first line of the paragraph.
  if (!recover) {
    screenIsFull = screenIsFull || ((pos.y + fmt.marginTop +
                                     (fmt.lineHeightFactor * font->getLineHeight(fmt.fontSize)) -
                                     font->getDescenderHeight(fmt.fontSize)) > maxY);
    if (screenIsFull) {
      return false;
    }
  }

  lineHeightFactor = fmt.lineHeightFactor;

  paraMinX = minX + fmt.marginLeft;
  paraMaxX = maxX - fmt.marginRight;

  if (fmt.indent < 0) {
    paraMinX -= fmt.indent;
  }

  // When recover == true, that means we are recovering the end of a paragraph that appears at the
  // beginning of a page. topMargin and indent must then be forgot as the have already been used at
  // the end of the page before...

  if (recover) {
    paraIndent = topMargin = 0;
  } else {
    paraIndent = fmt.indent;
    topMargin  = fmt.marginTop;
  }

  return true;
}

auto Page::breakParagraph(const Format &fmt) -> void {
  if (!lineList->empty()) {
    addLine(fmt, true);
  }
}

auto Page::endParagraph(const Format &fmt) -> bool {
  FontPtr &font = fonts.getFont(fmt.fontIndex);

  if (!lineList->empty()) {
    addLine(fmt, false);

    int32_t descender = font->getDescenderHeight(fmt.fontSize);

    pos.y += fmt.marginBottom; // - descender;

    if ((pos.y - descender) >= maxY) {
      screenIsFull = true;
      return false;
    }
  }

  return true;
}

auto Page::addLine(const Format &fmt, bool justifyable) -> void {
  if (pos.y == 0) pos.y = minY;

  // lineList->show("LINE");

  pos.x = paraMinX + paraIndent;
  if (pos.x < minX) pos.x = minX;
  int16_t lineHeight = glyphsHeight * lineHeightFactor;
  pos.y += topMargin + lineHeight; // (lineHeight >> 1) + (glyphsHeight >> 1);

  // #if DEBUGGING_AID
  //   if (show_location) {
  //     std::cout << "New Line: ";
  //     showControls(" ");
  //     showFmt(fmt, "   ->  ");
  //   }
  // #endif

  // Get rid of space characters that are at the end of the line.
  // This is mainly required for the JUSTIFY alignment algo.

  while (!lineList->empty()) {
    DisplayListEntry *entry = lineList->last();
    if ((entry->command == DisplayListCommand::GLYPH) && (std::get<GlyphEntry>(entry->v).isSpace)) {
      lineList->removeLast(); // This is O(N)...
    } else {
      break;
    }
  }

  if (!lineList->empty() && (computeMode == ComputeMode::DISPLAY)) {

    if ((fmt.align == CSS::Align::JUSTIFY) && justifyable) {
      int16_t targetWidth = (paraMaxX - paraMinX - paraIndent);
      int16_t loopCount   = 0;
      while ((lineWidth < targetWidth) && (++loopCount < 50)) {
        bool atLeastOnce = false;
        for (auto *entry : *lineList) {
          if (entry->pos.x > 0) { // This means it's a white space
            atLeastOnce = true;
            entry->pos.x++;
            if (++lineWidth >= targetWidth) break;
          }
        }
        if (!atLeastOnce) break; // No space available in line to justify the line
      }
      if (loopCount >= 50) {
        for (auto *entry : *lineList) entry->pos.x = 0;
      }
    } else {
      if (fmt.align == CSS::Align::RIGHT) {
        pos.x = paraMaxX - lineWidth;
      } else if (fmt.align == CSS::Align::CENTER) {
        pos.x = paraMinX + ((paraMaxX - paraMinX) >> 1) - (lineWidth >> 1);
      }
    }
  }

  for (auto *entry : *lineList) {
    if (entry->command == DisplayListCommand::GLYPH) {
      int16_t x    = entry->pos.x; // x may contains the calculated gap between words
      entry->pos.x = pos.x + std::get<GlyphEntry>(entry->v).glyph->xoff;
      entry->pos.y += pos.y + std::get<GlyphEntry>(entry->v).glyph->yoff;
      pos.x += (x == 0) ? std::get<GlyphEntry>(entry->v).kern : x;
    } else if (entry->command == DisplayListCommand::PICTURE) {
      if (fmt.align == CSS::Align::CENTER) {
        entry->pos.x = paraMinX + ((paraMaxX - paraMinX) >> 1) - (lineWidth >> 1);
      } else {
        entry->pos.x = pos.x;
      }
      entry->pos.y = pos.y - std::get<PictureEntry>(entry->v).picture->getDim().height;
      pos.x += std::get<PictureEntry>(entry->v).advance;
    } else {
      LOG_E("Wrong entry type for addLine: %d", (int)entry->command);
    }

    #if DEBUGGING
      // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
      //   LOG_E("addLine entry with a negative location: %d %d %d", entry->pos.x, entry->pos.y,
      //   (int)entry->command); showControls("  -> "); showFmt(fmt, "  -> ");
      // }
      // else
      if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
        LOG_E("addLine with a too large location: %d %d %d", entry->pos.x, entry->pos.y,
              (int)entry->command);
        showControls("  -> ");
        showFmt(fmt, "  -> ");
      }
    #endif
  }

  lineWidth = lineHeight = glyphsHeight = 0;
  lineHeightFactor                      = 0.0;

  displayList->merge(*lineList);

  paraIndent = 0;
  topMargin  = 0;

  // std::cout << "End New Line:"; showControls();
}

inline auto Page::addGlyphToLine(Glyph *glyph, const Format &fmt, Font &font, bool isSpace)
    -> void {
  if (isSpace && (lineWidth == 0)) return;

  DisplayListEntry *entry = lineList->getNewEntry();
  if (entry == nullptr) return;

  entry->command = DisplayListCommand::GLYPH;
  entry->v       = GlyphEntry{glyph, glyph->advance, isSpace};
  entry->pos     = {(uint16_t)(isSpace ? glyph->advance : 0), 0};

  if (glyphsHeight < glyph->lineHeight) glyphsHeight = glyph->lineHeight;
  if (lineHeightFactor < fmt.lineHeightFactor) lineHeightFactor = fmt.lineHeightFactor;

  lineWidth += (glyph->advance);

  lineList->pushBack(entry);
}

auto Page::addPictureToLine(PicturePtr picture, int16_t advance, const Format &fmt) -> void {
  DisplayListEntry *entry = lineList->getNewEntry();
  if (entry == nullptr) return;

  auto dim = picture->getDim();

  entry->command = DisplayListCommand::PICTURE;
  entry->v       = PictureEntry{std::move(picture), advance};

  // if (computeMode == ComputeMode::DISPLAY) {
  //   if (copy) {
  //     int32_t size = picture.dim.width * picture.dim.height;
  //     if (picture.bitmap != nullptr) {
  //       if ((entry->kind.picture_entry.picture.bitmap = new unsigned char[size]) == nullptr) {
  //         msg_viewer.outOfMemory("picture bitmap allocation");
  //       }
  //       memcpy((void *) entry->kind.picture_entry.picture.bitmap, picture.bitmap, size);
  //     }
  //     else {
  //       entry->kind.picture_entry.picture.bitmap = nullptr;
  //     }
  //   }
  //   else {
  //     entry->kind.picture_entry.picture.bitmap = picture.bitmap;
  //   }
  // }
  // else {
  //   entry->kind.picture_entry.picture.bitmap = nullptr;
  // }

  entry->pos = {0, 0};

  if (lineHeightFactor < fmt.lineHeightFactor) lineHeightFactor = fmt.lineHeightFactor;
  if (glyphsHeight < dim.height) glyphsHeight = dim.height / lineHeightFactor;

  lineWidth += advance;

  // LOG_D(
  //   "Picture added to line: w:%d h:%d a:%d",
  //   picture.width, picture.height,
  //   entry->kind.picture_entry.advance
  // );

  lineList->pushBack(entry);
}

#define NEXT_LINE_REQUIRED_SPACE                                                                   \
  (pos.y + (fmt.lineHeightFactor * font->getLineHeight(fmt.fontSize)) -                            \
   font->getDescenderHeight(fmt.fontSize))

#if 1
  auto Page::addWord(const char *word, const Format &fmt) -> bool {
    FontPtr &font = fonts.getFont(fmt.fontIndex);

    if (lineList->empty()) {
      // We are about to start a new line. Check if it will fit on the page.
      if ((screenIsFull = NEXT_LINE_REQUIRED_SPACE > maxY)) return false;
    }

    Glyph *glyph;

    auto lineEntries = DisplayList::Make(displayListPool);
    const char *str  = word;
    int16_t height   = font->getLineHeight(fmt.fontSize);
    int16_t width    = 0;
    bool first       = true;

    while (*str) {
      bool ignoreNext;
      const char *str1, *str2;
      uint32_t uc1, uc2;
      int16_t kern;

      uc1 = toUnicode(str, fmt.textTransform, first, &str1);
      uc2 = toUnicode(str1, fmt.textTransform, false, &str2);

      glyph = font->getGlyph(uc1, uc2, fmt.fontSize, kern, ignoreNext);

      str = ignoreNext ? str2 : str1;

      int16_t advance = kern;

      if (glyph == nullptr) {
        glyph = font->getGlyph(' ', fmt.fontSize);
        if (glyph != nullptr) {
          advance = glyph->advance;
        }
      }

      if (glyph != nullptr) {
        width += advance;
        first = false;

        DisplayListEntry *entry = lineEntries->getNewEntry();
        if (entry == nullptr) return false;

        entry->command = DisplayListCommand::GLYPH;
        entry->v       = GlyphEntry{glyph, advance, false};
        entry->pos     = {0, fmt.verticalAlign};

        lineEntries->pushBack(entry);
      }
    }

    uint16_t availableWidth = paraMaxX - paraMinX - paraIndent;

    if (width >= availableWidth) {
      if (strncasecmp(word, "http", 4) == 0) {
        return addWord("[URL removed]", fmt);
      } else {
        LOG_E("WORD TOO LARGE!! '%s'", word);
      }
    }

    if ((lineWidth + width) >= availableWidth) {
      addLine(fmt, true);
      screenIsFull = NEXT_LINE_REQUIRED_SPACE > maxY;
      if (screenIsFull) {
        return false;
      }
    }

    lineList->merge(*lineEntries.get());

    if (glyphsHeight < height) glyphsHeight = height;
    if (lineHeightFactor < fmt.lineHeightFactor) lineHeightFactor = fmt.lineHeightFactor;
    lineWidth += width;

    return true;
  }
#else
  // ToDo: Optimize this method...
  auto Page::addWord(const char *word, const Format &fmt) -> bool {
    Glyph *glyph;
    const char *str = word;
    int32_t code;

    FontPtr &font = fonts.getFont(fmt.fontIndex, fmt.fontSize);

    if (lineList->empty()) {
      // We are about to start a new line. Check if it will fit on the page.
      screenIsFull = NEXT_LINE_REQUIRED_SPACE > maxY;
      if (screenIsFull) {
        return false;
      }
    }

    int16_t width = 0;
    bool first = true;
    while (*str) {
      const char *s1;
      glyph = font->getGlyph(code = toUnicode(str, fmt.textTransform, first, &s1), fmt.fontSize);
      str = s1;
      if (glyph != nullptr) width += glyph->advance;
      first = false;
    }

    if (width >= (paraMaxX - paraMinX - paraIndent)) {
      if (strncasecmp(word, "http", 4) == 0) {
        return addWord("[URL removed]", fmt);
      } else {
        LOG_E("WORD TOO LARGE!! '%s'", word);
      }
    }

    if ((lineWidth + width) >= (paraMaxX - paraMinX - paraIndent)) {
      addLine(fmt, true);
      screenIsFull = NEXT_LINE_REQUIRED_SPACE > maxY;
      if (screenIsFull) {
        return false;
      }
    }

    first = true;
    while (*word) {
      if (font) {
        const char *s1;
        glyph = font->getGlyph(toUnicode(word, fmt.textTransform, first, &s1), fmt.fontSize);
        word = s1;
        if (glyph == nullptr) {
          glyph = font->getGlyph(' ', fmt.fontSize);
        }
        if (glyph != nullptr) {
          addGlyphToLine(glyph, fmt.fontSize, *font, false);
          // font->show_glyph(*glyph);
        }
      }
      first = false;
    }

    return true;
  }
#endif

auto Page::addChar(const char *ch, const Format &fmt) -> bool {
  Glyph *glyph;

  FontPtr &font = fonts.getFont(fmt.fontIndex);

  if (screenIsFull) return false;

  if (lineList->empty()) {

    // We are about to start a new line. Check if it will fit on the page.

    screenIsFull = ((pos.y + (fmt.lineHeightFactor * font->getLineHeight(fmt.fontSize)) -
                     font->getDescenderHeight(fmt.fontSize))) > maxY;
    if (screenIsFull) {
      return false;
    }
  }

  const char *s1;
  int32_t code = toUnicode(ch, fmt.textTransform, true, &s1);

  glyph = font->getGlyph(code, fmt.fontSize);

  if (glyph != nullptr) {
    // Verify that there is enough space for the glyph on the line.
    if ((lineWidth + (glyph->advance)) >= (paraMaxX - paraMinX - paraIndent)) {
      addLine(fmt, true);
      screenIsFull = NEXT_LINE_REQUIRED_SPACE > maxY;
      if (screenIsFull) {
        return fmt.trim;
      }
    }

    addGlyphToLine(glyph, fmt, *font, (code == 32) || (code == 160));
  }

  return true;
}

/**
 * @brief Adds an picture to the current page with automatic sizing and positioning.
 *
 * This method attempts to place an picture on the current page, automatically
 * calculating appropriate dimensions based on available space and format
 * specifications. If the picture doesn't fit on the current line, a new line
 * is created. The picture is resized if necessary when in DISPLAY compute mode.
 *
 * @param picture A reference to the Picture object to be added to the page.
 *              The picture may be resized in-place if dimensions don't match target size.
 * @param fmt A reference to the Format object containing formatting directives
 *            such as font index, font size, text transform, and optional width/height
 *            constraints.
 *
 * @return true if the picture was successfully added to the page; false if the screen
 *         is full or if picture dimensions become zero during resizing.
 *
 * @details
 * - Calculates the gap between glyph advance and glyph width for proper spacing
 * - Determines target picture dimensions based on available paragraph space and format constraints
 * - Maintains aspect ratio when scaling pictures
 * - Creates a new line if the picture exceeds remaining space on the current line
 * - Only performs actual picture resizing when computeMode is DISPLAY
 * - Sets screenIsFull flag if adding the picture causes content to exceed maxY
 *
 * @note The picture object may be modified (resized) by this method.
 *       The method respects glyph baseline information from the current font.
 */
auto Page::addPicture(PicturePtr picture, const Format &fmt /*, bool at_start_of_page*/)
    -> std::pair<bool, PicturePtr> {
  if (screenIsFull) {
    return {false, std::move(picture)};
  }

  // Compute the baseline advance for the bitmap, using info from the current font
  Glyph *glyph;
  FontPtr &font = fonts.getFont(fmt.fontIndex);

  const char *str = "m";
  const char *s1;
  int32_t code = toUnicode(str, fmt.textTransform, true, &s1);

  glyph = font->getGlyph(code, fmt.fontSize);

  // Compute available space to put the picture.

  int32_t w = 0;
  int32_t h = 0;
  int32_t advance;
  int16_t gap = 0;

  if (glyph != nullptr) {
    gap = glyph->advance - glyph->dim.width;
  }

  // compute target w, h and advance for the picture

  int16_t targetWidth  = paraMaxX - paraMinX;
  int16_t targetHeight = maxY - minY;
  auto dim             = picture->getDim();

  if (fmt.width || fmt.height) {
    if (fmt.width && (fmt.width < targetWidth)) targetWidth = fmt.width;
    if (fmt.height && (fmt.height < targetHeight)) targetHeight = fmt.height;

    w = targetWidth;
    h = dim.height * w / dim.width;
    if (h > targetHeight) {
      h = targetHeight;
      w = dim.width * h / dim.height;
    }
  } else {
    if ((dim.width < targetWidth) && (dim.height < targetHeight)) {
      w = dim.width;
      h = dim.height;
    } else {
      w = targetWidth;
      h = dim.height * w / dim.width;
      if (h > targetHeight) {
        h = targetHeight;
        w = dim.width * h / dim.height;
      }
    }
  }
  advance = w + gap;

  // Verify that there is enough room for the bitmap on the line

  if ((lineWidth + advance) >= (paraMaxX - paraMinX - paraIndent)) {
    //    if (!(lineList->empty() && at_start_of_page)) {
    addLine(fmt, true);
    // }
    // else {
    //   paraIndent = 0;
    //   topMargin = 0;
    // }

    // int16_t the_height = (fmt.lineHeightFactor * font->getLineHeight()) -
    // font->getDescenderHeight(); if (the_height < h) the_height = h;
  }

  if ((screenIsFull = ((pos.y + h) > maxY))) return {false, std::move(picture)};

  if ((w != dim.width) || (h != dim.height)) {

    // unsigned char * resized_bitmap = nullptr;

    if (computeMode == ComputeMode::DISPLAY) {
      if ((dim.width > 2) || (dim.height > 2)) {

        if ((w == 0) || (h == 0)) return {false, std::move(picture)};

        picture->resize(Dim(w, h));
      }
    }
  }

  addPictureToLine(std::move(picture), advance, fmt);

  return {true, nullptr};
}

/**
 * @brief Adds text to the page with the specified formatting.
 *
 * Processes the input string character by character, separating it into words
 * and whitespace. Words are added via addWord(), while individual spaces are
 * added as characters. The method respects Format settings and handles trimming.
 *
 * @param str The text string to add to the page.
 * @param fmt The Format object containing styling and layout information
 *            (font, size, color, trim behavior, etc.).
 *
 * @note The method allocates a temporary 100-byte buffer for word processing.
 *       If allocation fails, a message is sent via msg_viewer.outOfMemory().
 *       Processing stops if addChar() or addWord() returns false.
 *
 * @see addWord()
 * @see addChar()
 */
auto Page::addText(const std::string &str, const Format &fmt) -> void {
  Format myfmt = fmt;

  std::unique_ptr<char[]> buff = std::make_unique<char[]>(100);

  if (buff == nullptr) MsgViewer::outOfMemory("temp buffer allocation");
  const char *s = str.c_str();
  while (*s) {
    if (uint8_t(*s) <= ' ') {
      if (*s == ' ') {
        myfmt.trim = *s == ' ';
        if (!addChar(s, myfmt)) break;
      }
      s++;
    } else {
      int16_t count = 0;
      while ((uint8_t(*s) > ' ') && (count < 99)) {
        buff[count++] = *s++;
      }
      buff[count] = 0;
      if (!addWord(buff.get(), myfmt)) break;
    }
  }
}

auto Page::putPicture(PicturePtr picture, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) return;

  if (computeMode == ComputeMode::DISPLAY) {
    entry->v = PictureEntry{std::move(picture), 0};
  }
  entry->command = DisplayListCommand::PICTURE;
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("draw_bitmap with a negative location: %d %d", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("draw_bitmap with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::putHighlight(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) return;

  entry->command = DisplayListCommand::HIGHLIGHT;
  entry->v       = RegionEntry{dim};
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("putHighlight with a negative location: %d %d", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("putHighlight with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::clearHighlight(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) return;

  entry->command = DisplayListCommand::CLEAR_HIGHLIGHT;
  entry->v       = RegionEntry{dim};
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::putRounded(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) return;

  entry->command = DisplayListCommand::ROUNDED;
  entry->v       = RegionEntry{dim};
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("putHighlight with a negative location: %d %d", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("putHighlight with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::clearRounded(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) return;

  entry->command = DisplayListCommand::CLEAR_ROUNDED;
  entry->v       = RegionEntry{dim};
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::clearRegion(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) return;

  entry->command = DisplayListCommand::CLEAR_REGION;
  entry->v       = RegionEntry{dim};
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::setRegion(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) return;

  entry->command = DisplayListCommand::SET_REGION;
  entry->v       = RegionEntry{dim};
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::showCover(PicturePtr &pict) -> bool {
  if (computeMode == ComputeMode::DISPLAY) {
    int32_t picture_width  = pict->getDim().width;
    int32_t picture_height = pict->getDim().height;

    if (pict->getBitmap() != nullptr) {
      // LOG_D("Picture: width: %d height: %d channel_count: %d", picture_width, picture_height,
      // channel_count);

      Dim dim;
      Pos pos;

      dim.width  = Screen::getWidth();
      dim.height = picture_height * Screen::getWidth() / picture_width;

      if (dim.height > Screen::getHeight()) {
        dim.height = Screen::getHeight();
        dim.width  = picture_width * Screen::getHeight() / picture_height;
      }

      pos = {(uint16_t)((Screen::getWidth() - dim.width) >> 1),
             (uint16_t)((Screen::getHeight() - dim.height) >> 1)};

      pict->resize(dim);

      screen.clear();
      screen.drawPicture(pict, pos);
      screen.update();
    } else {
      LOG_D("Unable to load cover file");
      return false;
    }

    return true;
  }

  return false;
}

auto Page::getPixelValue(const CSS::Value &value, const Format &fmt, int16_t ref, bool vertical)
    -> int16_t {
  switch (value.valueType) {
  case CSS::ValueType::PX:
    return value.num;
  case CSS::ValueType::PT:
    return (value.num * Screen::RESOLUTION) / 72;
  case CSS::ValueType::EM:
  case CSS::ValueType::REM:
    return value.num * ((fmt.fontSize * Screen::RESOLUTION) / 72);
  case CSS::ValueType::PERCENT:
    return (value.num * ref) / 100;
  case CSS::ValueType::NO_TYPE:
    return ref * value.num;
  case CSS::ValueType::CM:
    return (value.num * Screen::RESOLUTION) / 2.54;
  case CSS::ValueType::VH:
    return (value.num * (fmt.screenBottom - fmt.screenTop)) / 100;
  case CSS::ValueType::VW:
    return (value.num * (fmt.screenRight - fmt.screenLeft)) / 100;
    ;
  case CSS::ValueType::STR:
    // LOG_D("getPixelValue(): Str value: %s", value.str.c_str());
    return 0;
  default:
    // LOG_D("getPixelValue: Wrong data type!: %d", value.valueType);
    return value.num;
  }
  return 0;
}

auto Page::getPointValue(const CSS::Value &value, const Format &fmt, int16_t ref) -> int16_t {
  // int8_t normal_size = epub.getBookFormatParams()->fontSize;
  int16_t normal_size = ref;

  switch (value.valueType) {
  case CSS::ValueType::PX:
    //  pixels -> Screen HResolution per inch
    //    x    ->    72 pixels per inch
    return (value.num * 72) / Screen::RESOLUTION; // convert pixels in points
  case CSS::ValueType::PT:
    return value.num; // Already in points
  case CSS::ValueType::EM:
  case CSS::ValueType::REM:
    return value.num * ref;
  case CSS::ValueType::CM:
    return (value.num * 72) / 2.54;
  case CSS::ValueType::PERCENT:
    return (value.num * normal_size) / 100;
  case CSS::ValueType::NO_TYPE:
    return normal_size * value.num;
  case CSS::ValueType::STR:
    LOG_D("getPointValue(): Str value: %s.", value.str.c_str());
    return 0;
    break;
  case CSS::ValueType::VH:
    return ((value.num * (fmt.screenBottom - fmt.screenTop)) / 100) * 72 / Screen::RESOLUTION;
  case CSS::ValueType::VW:
    return ((value.num * (fmt.screenRight - fmt.screenLeft)) / 100) * 72 / Screen::RESOLUTION;
  case CSS::ValueType::INHERIT:
    return ref;
  default:
    LOG_E("getPointValue(): Wrong data type!");
    return value.num;
  }
  return 0;
}

auto Page::getFactorValue(const CSS::Value &value, const Format &fmt, float ref) -> float {
  switch (value.valueType) {
  case CSS::ValueType::PX:
  case CSS::ValueType::PT:
  case CSS::ValueType::STR:
    return 1.0;
  case CSS::ValueType::EM:
  case CSS::ValueType::REM:
  case CSS::ValueType::NO_TYPE:
  case CSS::ValueType::DIMENSION:
    return value.num;
  case CSS::ValueType::PERCENT:
    return (value.num * ref) / 100.0;
  case CSS::ValueType::INHERIT:
    return ref;
  default:
    // LOG_E("getFactorValue: Wrong data type!");
    return 1.0;
  }
  return 0;
}

void Page::adjustFormat(DOM::Node *domCurrentNode, Format &fmt, const CSSPtr &elementCss,
                        const CSSPtr &itemCss) {
  CSS::RulesMap rules;

  // itemCss->show();
  // std::cout << "======" << std::endl;
  // if (elementCss != nullptr) elementCss->show();
  // std::cout << "------" << std::endl;
  // domCurrentNode->show(1);

  if (itemCss != nullptr) {
    itemCss->match(domCurrentNode, rules);
    if (!rules.empty()) {
      // itemCss->show(rules);
      adjustFormatFromRules(fmt, rules);
    } else {
      #if DEBUGGING1
        LOG_D("No match");
        domCurrentNode->show(1);
      #endif
    }
  }
  if (elementCss != nullptr) {
    if (!elementCss->rulesMap.empty()) adjustFormatFromRules(fmt, elementCss->rulesMap);
  }
}

auto Page::adjustFormatFromRules(Format &fmt, const CSS::RulesMap &rules) -> void {
  const CSS::Values *vals;

  // LOG_D("Found!");

  FaceStyle fontWeight =
      ((fmt.fontStyle == FaceStyle::BOLD) || (fmt.fontStyle == FaceStyle::BOLD_ITALIC))
          ? FaceStyle::BOLD
          : FaceStyle::NORMAL;
  FaceStyle fontStyle =
      ((fmt.fontStyle == FaceStyle::ITALIC) || (fmt.fontStyle == FaceStyle::BOLD_ITALIC))
          ? FaceStyle::ITALIC
          : FaceStyle::NORMAL;

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::FONT_STYLE))) {
    fontStyle = (FaceStyle)vals->front()->choice.faceStyle;
  }
  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::FONT_WEIGHT))) {
    fontWeight = (FaceStyle)vals->front()->choice.faceStyle;
  }

  FaceStyle newStyle = fonts.adjustFontStyle(fmt.fontStyle, fontStyle, fontWeight);

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::FONT_FAMILY))) {
    int16_t idx = -1;
    for (auto &fontName : *vals) {
      if ((idx = fonts.getIndex(fontName->str, newStyle)) != -1) break;
    }
    if (idx == -1) {
      LOG_D("Font not found 1: %s %d", vals->front()->str.c_str(), (int)newStyle);
      idx = fonts.getIndex("Default", newStyle);
    }
    if (idx == -1) {
      fmt.fontStyle = FaceStyle::NORMAL;
      fmt.fontIndex = 3;
    } else {
      // LOG_D("Font index: %d", idx);
      fmt.fontIndex = idx;
      fmt.fontStyle = newStyle;
    }
  } else if (newStyle != fmt.fontStyle) {
    resetFontIndex(fmt, newStyle);
  }

  if (!fonts.check(fmt.fontIndex, fmt.fontStyle)) {
    fmt.fontStyle = FaceStyle::NORMAL;
    fmt.fontIndex = 3;
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::TEXT_ALIGN))) {
    fmt.align = (CSS::Align)vals->front()->choice.align;
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::TEXT_INDENT))) {
    fmt.indent = getPixelValue(*(vals->front()), fmt, paintWidth());
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::FONT_SIZE))) {
    fmt.fontSize = getPointValue(*(vals->front()), fmt, fmt.fontSize);
    if (fmt.fontSize == 0) {
      LOG_E("adjustFormatFromSuite: setting fmt.fontSize to 0!!!");
    }
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::LINE_HEIGHT))) {
    fmt.lineHeightFactor = getFactorValue(*(vals->front()), fmt, fmt.lineHeightFactor);
  }

  int16_t widthRef  = Screen::getWidth() - fmt.screenLeft - fmt.screenRight;
  int16_t heightRef = Screen::getHeight() - fmt.screenTop - fmt.screenBottom;

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN))) {

    int16_t size = 0;
    for (auto val __attribute__((unused)) : *vals) size++;
    CSS::Values::const_iterator it = vals->begin();

    if (size == 1) {
      fmt.marginTop = fmt.marginBottom = getPixelValue(*(vals->front()), fmt, heightRef, true);
      fmt.marginRight = fmt.marginLeft = getPixelValue(*(vals->front()), fmt, widthRef);
    } else if (size == 2) {
      fmt.marginTop = fmt.marginBottom = getPixelValue(**it++, fmt, heightRef, true);
      fmt.marginRight = fmt.marginLeft = getPixelValue(**it, fmt, widthRef);
    } else if (size == 3) {
      fmt.marginTop   = getPixelValue(**it++, fmt, heightRef, true);
      fmt.marginRight = fmt.marginLeft = getPixelValue(**it++, fmt, widthRef);
      fmt.marginBottom                 = getPixelValue(**it, fmt, heightRef, true);
    } else if (size == 4) {
      fmt.marginTop    = getPixelValue(**it++, fmt, heightRef, true);
      fmt.marginRight  = getPixelValue(**it++, fmt, widthRef);
      fmt.marginBottom = getPixelValue(**it++, fmt, heightRef, true);
      fmt.marginLeft   = getPixelValue(**it, fmt, widthRef);
    }
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::DISPLAY))) {
    fmt.display = vals->front()->choice.display;
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN_LEFT))) {
    fmt.marginLeft = getPixelValue(*(vals->front()), fmt, widthRef);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN_RIGHT))) {
    fmt.marginRight = getPixelValue(*(vals->front()), fmt, widthRef);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN_TOP))) {
    fmt.marginTop = getPixelValue(*(vals->front()), fmt, heightRef, true);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN_BOTTOM))) {
    fmt.marginBottom = getPixelValue(*(vals->front()), fmt, heightRef, true);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::WIDTH))) {
    fmt.width = getPixelValue(*(vals->front()), fmt,
                              fmt.width /*Screen::getWidth() - fmt.screenLeft - fmt.screenRight */);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::HEIGHT))) {
    fmt.height = getPixelValue(*(vals->front()), fmt,
                               Screen::getHeight() - fmt.screenTop - fmt.screenBottom);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::TEXT_TRANSFORM))) {
    fmt.textTransform = vals->front()->choice.textTransform;
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::VERTICAL_ALIGN))) {
    if (vals->front()->choice.verticalAlign == CSS::VerticalAlign::NORMAL) {
      fmt.verticalAlign = 0;
    } else if (vals->front()->choice.verticalAlign == CSS::VerticalAlign::SUB) {
      fmt.verticalAlign = 5;
    } else if (vals->front()->choice.verticalAlign == CSS::VerticalAlign::SUPER) {
      fmt.verticalAlign = -5;
    } else if (vals->front()->choice.verticalAlign == CSS::VerticalAlign::VALUE) {
      FontPtr &font = fonts.getFont(fmt.fontIndex);

      fmt.verticalAlign = -getPixelValue(*(vals->front()), fmt, font->getLineHeight(fmt.fontSize));
    }
  }
}
