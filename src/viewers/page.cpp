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
#include <utility>

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
    const uint8_t *s    = name;
    uint8_t len         = 0;
    while ((len < 7) && (*s != 0) && (*s != ';')) {
      ++s;
      ++len;
    }

    if (*s == ';') {
      switch (len) {
      case 2:
        if (strncmp("lt;", reinterpret_cast<const char *>(name), 3) == 0)
          u = 60;
        else if (strncmp("gt;", reinterpret_cast<const char *>(name), 3) == 0)
          u = 62;
        break;
      case 3:
        if (strncmp("amp;", reinterpret_cast<const char *>(name), 4) == 0) u = 38;
        break;
      case 4:
        if (strncmp("nbsp;", reinterpret_cast<const char *>(name), 5) == 0)
          u = 160;
        else if (strncmp("quot;", reinterpret_cast<const char *>(name), 5) == 0)
          u = 34;
        else if (strncmp("apos;", reinterpret_cast<const char *>(name), 5) == 0)
          u = 39;
        else if (strncmp("euro;", reinterpret_cast<const char *>(name), 5) == 0)
          u = 0x20AC;
        else if (strncmp("copy;", reinterpret_cast<const char *>(name), 5) == 0)
          u = 0x00A9;
        break;
      case 5:
        if (strncmp("mdash;", reinterpret_cast<const char *>(name), 6) == 0)
          u = 0x2014;
        else if (strncmp("ndash;", reinterpret_cast<const char *>(name), 6) == 0)
          u = 0x2013;
        else if (strncmp("lsquo;", reinterpret_cast<const char *>(name), 6) == 0)
          u = 0x2018;
        else if (strncmp("rsquo;", reinterpret_cast<const char *>(name), 6) == 0)
          u = 0x2019;
        else if (strncmp("ldquo;", reinterpret_cast<const char *>(name), 6) == 0)
          u = 0x201C;
        else if (strncmp("rdquo;", reinterpret_cast<const char *>(name), 6) == 0)
          u = 0x201D;
        break;
      case 6:
        if (strncmp("dagger;", reinterpret_cast<const char *>(name), 7) == 0)
          u = 0x2020;
        else if (strncmp("Dagger;", reinterpret_cast<const char *>(name), 7) == 0)
          u = 0x2021;
        break;
      default:
        break;
      }
    }

    if (u == ' ') {
      // Unknown or malformed entity: consume '&' and return it as-is.
      u = '&';
      ++c;
    } else {
      c = s + 1;
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

  Font *font = fonts.get(fmt.fontIndex);

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

  Font *font = fonts.get(fmt.fontIndex);

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

  paraIndent = 0;
  lineWidth  = 0;
}

auto Page::setLimits(const Format &fmt) -> void {
  pos.x = pos.y = 0;

  minY = fmt.screenTop;
  maxX = Screen::getWidth() - fmt.screenRight;
  maxY = Screen::getHeight() - fmt.screenBottom;
  minX = fmt.screenLeft;

  screenIsFull = false;

  lineList->clear();

  paraIndent = 0;
  lineWidth  = 0;
  topMargin  = 0;
}

auto Page::lineBreak(const Format &fmt, int8_t indentNextLine) -> bool {
  Font *font = fonts.get(fmt.fontIndex);

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
  Font *font = fonts.get(fmt.fontIndex);

  // Check if there is enough room for the first line of the paragraph.
  if (!recover) {
    screenIsFull = screenIsFull || ((pos.y + fmt.marginTop +
                                     (fmt.lineHeightFactor * font->getLineHeight(fmt.fontSize)) -
                                     font->getDescenderHeight(fmt.fontSize)) > maxY);
    if (screenIsFull) {
      return false;
    }
  }

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
  Font *font = fonts.get(fmt.fontIndex);

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
    Font *font = fonts.get(fmt.fontIndex);
    if (font == nullptr) return false;

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

      if (glyph == nullptr) {
        glyph = font->getGlyph(' ', fmt.fontSize);
      }

      if (glyph != nullptr) {
        width += kern;
        first = false;

        DisplayListEntry *entry = lineEntries->getNewEntry();
        if (entry == nullptr) return false;

        entry->command = DisplayListCommand::GLYPH;
        entry->v       = GlyphEntry{glyph, kern, false};
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

    Font *font = fonts.get(fmt.fontIndex, fmt.fontSize);

    if (font == nullptr) return false;

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

  Font *font = fonts.get(fmt.fontIndex);

  if (font == nullptr) return false;

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
  Font *font = fonts.get(fmt.fontIndex);

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

  Fonts::FaceStyle fontWeight = ((fmt.fontStyle == Fonts::FaceStyle::BOLD) ||
                                 (fmt.fontStyle == Fonts::FaceStyle::BOLD_ITALIC))
                                    ? Fonts::FaceStyle::BOLD
                                    : Fonts::FaceStyle::NORMAL;
  Fonts::FaceStyle fontStyle  = ((fmt.fontStyle == Fonts::FaceStyle::ITALIC) ||
                                 (fmt.fontStyle == Fonts::FaceStyle::BOLD_ITALIC))
                                    ? Fonts::FaceStyle::ITALIC
                                    : Fonts::FaceStyle::NORMAL;

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::FONT_STYLE))) {
    fontStyle = (Fonts::FaceStyle)vals->front()->choice.faceStyle;
  }
  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::FONT_WEIGHT))) {
    fontWeight = (Fonts::FaceStyle)vals->front()->choice.faceStyle;
  }
  Fonts::FaceStyle newStyle = fonts.adjustFontStyle(fmt.fontStyle, fontStyle, fontWeight);

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
      fmt.fontStyle = Fonts::FaceStyle::NORMAL;
      fmt.fontIndex = 3;
    } else {
      // LOG_D("Font index: %d", idx);
      fmt.fontIndex = idx;
      fmt.fontStyle = newStyle;
    }
  } else if (newStyle != fmt.fontStyle) {
    resetFontIndex(fmt, newStyle);
  }

  fonts.check(fmt.fontIndex, fmt.fontStyle);

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
      Font *font = fonts.get(fmt.fontIndex);

      fmt.verticalAlign = -getPixelValue(*(vals->front()), fmt, font->getLineHeight(fmt.fontSize));
    }
  }
}
