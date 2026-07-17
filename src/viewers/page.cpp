// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "page.hpp"

#include "picture.hpp"

#include "alloc.hpp"
#include "screen.hpp"
#include "config.hpp"

#include "utf8.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

auto Page::clean() -> void {
  displayList->clear();
  lineList->clear();
  paraIndent   = 0;
  topMargin    = 0;
  screenIsFull = false;
  pageEmpty    = true;
}

auto Page::putStrAt(const std::string &str, Pos pos, const Format &fmt) -> void {
  FontPtr &   font = fonts.getFont(fmt.fontIndex);

  const char *s = str.c_str();

  if (fmt.align == HAlign::LEFT) {
    bool first = true;
    while (*s) {
      auto [ch, s1] = UTF8::toUnicode(s, fmt.textTransform, first);
      auto glyph = font->getGlyph(ch, fmt.fontSize);
      s     = s1;
      if (glyph != nullptr) {
        DisplayListEntry *entry = displayList->getNewEntry();
        if (entry == nullptr) { return; }
        entry->command = DisplayListCommand::GLYPH;
        entry->pos     = { static_cast<uint16_t>(pos.x + glyph->xoff),
                           static_cast<uint16_t>(pos.y + glyph->yoff) };
        entry->v       = GlyphEntry{ glyph, glyph->advance, false };

        #if DEBUGGING
          // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
          //   LOG_E("Put_str_at with a negative location: {} {}", entry->pos.x, entry->pos.y);
          // }
          // else
          if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
            LOG_E("Put_str_at with a too large location: {} {}", entry->pos.x, entry->pos.y);
          }
        #endif

        displayList->pushBack(entry);

        pos.x += glyph->advance;
      }
      first = false;
    }
  } else {
    int16_t     size  = 0;
    const char *s = str.c_str();

    while (*s) {
      bool        first = true;

      auto [ch, s1] = UTF8::toUnicode(s, fmt.textTransform, first);
      auto glyph = font->getGlyph(ch, fmt.fontSize);

      s     = s1;
      if (glyph != nullptr) { size += glyph->advance; }
      first = false;
    }

    int16_t x;

    if (pos.x == HORIZONTAL_CENTER) {
      if (fmt.align == HAlign::CENTER) {
        x = (screen.getWidth() >> 1) - (size >> 1);
        // x = minX + ((maxX - minX) >> 1) - (size >> 1);
      } else { // RIGHT
        x = maxX - size;
      }
    } else {
      if (fmt.align == HAlign::CENTER) {
        x = pos.x - (size >> 1);
      } else { // RIGHT
        x = pos.x - size;
      }
    }

    s = str.c_str();

    bool first = true;

    while (*s) {
      auto [ch, s1] = UTF8::toUnicode(s, fmt.textTransform, first);
      auto glyph = font->getGlyph(ch, fmt.fontSize);
      s     = s1;
      if (glyph != nullptr) {

        DisplayListEntry *entry = displayList->getNewEntry();
        if (entry == nullptr) { return; }

        entry->command = DisplayListCommand::GLYPH;
        entry->pos     = { static_cast<uint16_t>(x + glyph->xoff),
                           static_cast<uint16_t>(pos.y + glyph->yoff) };
        entry->v       = GlyphEntry{ glyph, glyph->advance, false };

        #if DEBUGGING
          // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
          //   LOG_E("Put_str_at with a negative location: {} {}", entry->pos.x, entry->pos.y);
          // }
          // else
          if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
            LOG_E("Put_str_at with a too large location: {} {}", entry->pos.x, entry->pos.y);
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
  Glyph *  glyph;

  FontPtr &font = fonts.getFont(fmt.fontIndex);

  glyph = font->getGlyph(ch, fmt.fontSize);
  if (glyph != nullptr) {
    DisplayListEntry *entry = displayList->getNewEntry();
    if (entry == nullptr) { return; }
    entry->command = DisplayListCommand::GLYPH;
    entry->pos     = { static_cast<uint16_t>(pos.x + glyph->xoff),
                       static_cast<uint16_t>(pos.y + glyph->yoff) };
    entry->v       = GlyphEntry{ glyph, glyph->advance, false };

    #if DEBUGGING
      // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
      //   LOG_E("Put_char_at with a negative location: {} {}", entry->pos.x, entry->pos.y);
      // }
      // else
      if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
        LOG_E("Put_char_at with a too large location: {} {}", entry->pos.x, entry->pos.y);
      }
    #endif

    displayList->pushBack(entry);
  }
}

auto Page::paint(bool clearScreen, bool noFull, bool doIt) -> void {
  if (!doIt) {
    if ((displayList->empty()) || (computeMode != ComputeMode::DISPLAY)) { return; }
  }

  // displayList->show("DISPLAY LIST");

  if (clearScreen) { screen.clear(); }

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

    // ++count;
  }

  // LOG_I("Painted {} entries on screen", count);

  screen.update(noFull);
}

auto Page::start(const Format &fmt, int8_t colCount) -> void {

  columnCount = colCount;
  multiColumnMode = (columnCount > 1) && (columnCount <= 4);

  minY = fmt.screenTop;
  maxY = Screen::getHeight() - fmt.screenBottom;

  minX = fmt.screenLeft;

  if (multiColumnMode) {
    currentColumn = 1;
    columnWidth = ((Screen::getWidth() - fmt.screenLeft - fmt.screenRight) -
                   (20 * (columnCount - 1))) / columnCount;
    maxX = minX + columnWidth;
  } else {
    maxX = Screen::getWidth() - fmt.screenRight;
  }

  pos.x = minX;
  pos.y = minY;

  paraMinX = minX;
  paraMaxX = maxX;

  screenIsFull = false;
  pageEmpty    = true;

  displayList->clear();
  lineList->clear();

  paraIndent   = 0;
  lineWidth    = 0;
  glyphsHeight = 0;

  // Recycle any cached glyph bitmaps left from the previous page.
  // fonts.clearGlyphCaches();
}

auto Page::setLimits(const Format &fmt) -> void {
  minY = fmt.screenTop;
  maxY = Screen::getHeight() - fmt.screenBottom;

  if (multiColumnMode) {
    minX = fmt.screenLeft + ((10 + columnWidth) * (currentColumn - 1));
    maxX = minX + columnWidth;
  } else {
    maxX = Screen::getWidth() - fmt.screenRight;
    minX = fmt.screenLeft;
  }

  pos.x = minX;
  pos.y = minY;

  screenIsFull = false;

  lineList->clear();

  paraIndent   = 0;
  lineWidth    = 0;
  topMargin    = 0;
  glyphsHeight = 0;
}

auto Page::lineBreak(const Format &fmt, int8_t indentNextLine) -> bool {
  FontPtr &font = fonts.getFont(fmt.fontIndex);

  if (!lineList->empty() || (computeMode != ComputeMode::DISPLAY && glyphsHeight > 0)) {
    addLine(fmt, false);
  } else {
    pos.y += font->getLineHeight(fmt.fontSize) * fmt.lineHeightFactor;
    pos.x = minX + indentNextLine;
  }

  if (theScreenIsFull(fmt, font, -font->getDescenderHeight(fmt.fontSize))) { return false;  }
  return true;
}

auto Page::newParagraph(const Format &fmt, bool recover) -> bool {
  FontPtr &font = fonts.getFont(fmt.fontIndex);

  lineHeightFactor = (pos.y == minY) ? fmt.lineHeightFactor : fmt.lineHeightFactor * 1.5;

  // Check if there is enough room for the first line of the paragraph.
  if (!recover) {
    if (theScreenIsFull(fmt, font)) { return false; }
  }

  paraMinX = minX + fmt.marginLeft;
  paraMaxX = maxX - fmt.marginRight;

  if (fmt.indent < 0) {
    paraMinX -= fmt.indent;
  }

  // When recover == true, that means we are recovering the end of a paragraph that appears at the
  // beginning of a page. topMargin and indent must then be forgot as the have already been used
  // at the end of the page before...

  if (recover) {
    paraIndent = topMargin = 0;
  } else {
    paraIndent = fmt.indent;
    topMargin  = fmt.marginTop;
  }

  return true;
}

auto Page::breakParagraph(const Format &fmt) -> void {
  if (!lineList->empty() || (computeMode != ComputeMode::DISPLAY && glyphsHeight > 0)) {
    addLine(fmt, true);
  }
}

auto Page::endParagraph(const Format &fmt) -> bool {
  FontPtr &font = fonts.getFont(fmt.fontIndex);

  if (!lineList->empty() || (computeMode != ComputeMode::DISPLAY && glyphsHeight > 0)) {
    addLine(fmt, false);

    int32_t descender = font->getDescenderHeight(fmt.fontSize);

    pos.y += fmt.marginBottom; // - descender;

    if (theScreenIsFull(fmt, font, -descender)) { return false; }
  }

  return true;
}

auto Page::addLine(const Format &fmt, bool justifyable) -> void {
  if (pos.y == 0) { pos.y = minY; }

  // lineList->show("LINE");

  pos.x = paraMinX + paraIndent;
  if (pos.x < minX) { pos.x = minX; }

  #if LINE_POS_TRACING
    if (tracing) { LOG_I("Line at [{}, {}]", pos.x, pos.y); }
  #endif

  int16_t lineHeight = glyphsHeight * lineHeightFactor;
  pos.y += topMargin + lineHeight; // (lineHeight >> 1) + (glyphsHeight >> 1);

  // if (pos.y > maxY) {
  //   LOG_E("We got a problem!");
  // }

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
    auto *entry = lineList->last();
    if ((entry->command == DisplayListCommand::GLYPH) && (std::get<GlyphEntry>(entry->v).isSpace)) {
      lineWidth -= std::get<GlyphEntry>(entry->v).glyph->advance;
      lineList->removeLast(); // This is O(N)...
    } else {
      break;
    }
  }

  if (!lineList->empty() && (computeMode == ComputeMode::DISPLAY)) {

    switch (fmt.align) {
    case HAlign::JUSTIFY:
      if (justifyable) {
        #if DEBUGGING_AID
          uint16_t computedLineWidth = 0;
          for (auto *entry : *lineList) {
            switch (entry->command) {
            case DisplayListCommand::GLYPH:
              computedLineWidth += std::get<GlyphEntry>(entry->v).glyph->advance;
              break;
            case DisplayListCommand::PICTURE:
              computedLineWidth += std::get<PictureEntry>(entry->v).advance;
              break;
            default:
              break;
            }
          }

          if (lineWidth != computedLineWidth) {
            LOG_W("lineWidth ({}) != computedLineWidth ({})!", lineWidth, computedLineWidth)
          }
        #endif

        auto *entry = lineList->last();
        if (entry->command == DisplayListCommand::GLYPH) {
          auto adj = (std::get<GlyphEntry>(entry->v).glyph->advance - 
                        std::get<GlyphEntry>(entry->v).glyph->dim.width);
          lineWidth -= adj;
        }

        int16_t targetWidth = (paraMaxX - paraMinX - paraIndent);
        int16_t loopCount   = 0;

        #if DEBUGGING_AID
          if (lineWidth > targetWidth) {
            LOG_W("lineWidth ({}) larger than targetWidth ({})!", lineWidth, targetWidth);
          }
        #endif

        while ((lineWidth < targetWidth) && (++loopCount < 100)) {
          bool atLeastOnce = false;
          for (auto *entry : *lineList) {
            if (entry->pos.x > 0) { // This means it's a white space
              atLeastOnce = true;
              ++entry->pos.x;
              if (++lineWidth > targetWidth) { break; }
            }
          }
          if (!atLeastOnce) { break; } // No space available in line to justify the line
        }
        if (loopCount >= 100) {
          for (auto *entry : *lineList) entry->pos.x = 0;
        }

        #if DEBUGGING_AID
          if (lineWidth != targetWidth) {
            LOG_W("Hum... lineWidth ({}) not equal to targetWidth ({})!", lineWidth, targetWidth);
          }
        #endif
      }
      break;

    case HAlign::RIGHT:
      pos.x = paraMaxX - lineWidth;
      break;

    case HAlign::CENTER:
      pos.x = paraMinX + ((paraMaxX - paraMinX) >> 1) - (lineWidth >> 1);
      break;

    default:
      break;
    }
  }

  for (auto *entry : *lineList) {
    switch (entry->command) {
    case DisplayListCommand::GLYPH: {
      int16_t x    = entry->pos.x; // x may contains the calculated gap between words
      entry->pos.x = pos.x + std::get<GlyphEntry>(entry->v).glyph->xoff;
      entry->pos.y += pos.y + std::get<GlyphEntry>(entry->v).glyph->yoff;
      pos.x += (x == 0) ? std::get<GlyphEntry>(entry->v).kern : x;
    }
    break;
    
    case DisplayListCommand::PICTURE:
      if (fmt.align == HAlign::CENTER) {
        entry->pos.x = paraMinX + ((paraMaxX - paraMinX) >> 1) - (lineWidth >> 1);
      } else {
        entry->pos.x = pos.x;
      }
      entry->pos.y = pos.y - std::get<PictureEntry>(entry->v).picture->getDim().height;
      pos.x += std::get<PictureEntry>(entry->v).advance;
      break;

    default:
      LOG_E("Wrong entry type for addLine: {}", (int)entry->command);
      break;
    }

    #if DEBUGGING_AID
      // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
      //   LOG_E("addLine entry with a negative location: {} {} {}", entry->pos.x, entry->pos.y,
      //   (int)entry->command); showControls("  -> "); showFmt(fmt, "  -> ");
      // }
      // else
      if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
        LOG_E("addLine with a too large location: {} {} {}", entry->pos.x, entry->pos.y,
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

  LOG_D("Y pos after addLine: {}", pos.y);
  // std::cout << "End New Line:"; showControls();
}

inline auto Page::addGlyphToLine(Glyph *glyph, const Format &fmt, Font &font, bool isSpace)
-> void {
  if (isSpace && (lineWidth == 0)) { return; }

  if (computeMode != ComputeMode::DISPLAY) {
    if (glyphsHeight < glyph->lineHeight) { glyphsHeight = glyph->lineHeight; }
    if (lineHeightFactor < fmt.lineHeightFactor) { lineHeightFactor = fmt.lineHeightFactor; }
    lineWidth += glyph->advance;
  } else {
    DisplayListEntry *entry = lineList->getNewEntry();
    if (entry == nullptr) { return; }

    entry->command = DisplayListCommand::GLYPH;
    entry->v       = GlyphEntry{ glyph, glyph->advance, isSpace };
    entry->pos     = { (uint16_t)(isSpace ? glyph->advance : 0), 0 };

    if (glyphsHeight < glyph->lineHeight) { glyphsHeight = glyph->lineHeight; }
    if (lineHeightFactor < fmt.lineHeightFactor) { lineHeightFactor = fmt.lineHeightFactor; }

    lineWidth += (glyph->advance);

    lineList->pushBack(entry);
  }

  pageEmpty = false;
}

auto Page::addPictureToLine(PicturePtr picture, int16_t advance, const Format &fmt) -> void {
  if (computeMode != ComputeMode::DISPLAY) {
    auto dim = picture->getDim();
    if (lineHeightFactor < fmt.lineHeightFactor) { lineHeightFactor = fmt.lineHeightFactor; }
    if (glyphsHeight < dim.height) { glyphsHeight = dim.height / lineHeightFactor; }
    lineWidth += advance;
  } else {

    DisplayListEntry *entry = lineList->getNewEntry();
    if (entry == nullptr) { return; }

    auto dim = picture->getDim();

    entry->command = DisplayListCommand::PICTURE;
    entry->v       = PictureEntry{ std::move(picture), advance };
    entry->pos     = { 0, 0 };

    if (lineHeightFactor < fmt.lineHeightFactor) { lineHeightFactor = fmt.lineHeightFactor; }
    if (glyphsHeight < dim.height) { glyphsHeight = dim.height / lineHeightFactor; }

    lineWidth += advance;

    // LOG_D(
    //   "Picture added to line: w:%d h:%d a:%d",
    //   picture.width, picture.height,
    //   entry->kind.picture_entry.advance
    // );

    lineList->pushBack(entry);
  }

  pageEmpty = false;
}

#define NEXT_LINE_REQUIRED_SPACE static_cast<uint16_t>                                     \
        ((lineHeightFactor * font->getLineHeight(fmt.fontSize)))

auto Page::theScreenIsFull(const Format &fmt, FontPtr &font, uint16_t verticalSizeNeeded) -> bool {

  if (screenIsFull) { return true; }

  if (lineHeightFactor == 0) { lineHeightFactor = fmt.lineHeightFactor; }

  auto yPos = (pos.y + (verticalSizeNeeded != 0 ? verticalSizeNeeded : NEXT_LINE_REQUIRED_SPACE));
  bool notEnoughRoom =  yPos > maxY;

  LOG_D("theColumnIsFull = {}, YPos = {}, maxY = {}", notEnoughRoom ? "Yes" : "No", yPos, maxY);

  if (notEnoughRoom) {
    if (multiColumnMode && (currentColumn < columnCount)) {
      ++currentColumn;

      minX = maxX + 20;
      maxX = minX + columnWidth;

      paraMinX = minX;
      paraMaxX = maxX;

      pos.x = minX;
      pos.y = minY;

      // In case the last line in preceeding column that
      // cause a changing of column was the beginning of
      // a paragraph.
      lineHeightFactor = fmt.lineHeightFactor;
    } else {
      screenIsFull = true;
      return true;
    }
  }

  return false;

}

#undef NEXT_LINE_REQUIRED_SPACE

auto Page::checkState(int ident, PageId &pageId) -> void {
  if ((lineWidth != 0) && screenIsFull) {
    LOG_E("Screen is full and lineWidth is not zero: {} pixels [{}, {}]! ({})", lineWidth, pageId.itemrefIndex, pageId.offset, ident);
  }
}

auto Page::addWord(const char *word, const Format &fmt, bool addToEndOfLine) -> const char * {

  FontPtr & font = fonts.getFont(fmt.fontIndex);

  if (strcmp(word, "continuent") == 0) {
    LOG_I("Found it!");
  }

  const char *str            = word;
  int16_t     height         = font->getLineHeight(fmt.fontSize);
  int16_t     wordWidth      = 0;
  bool        first          = true;
  uint16_t    availableWidth = paraMaxX - paraMinX - paraIndent;

  // The following are required when a word is too large to fit alone
  // on a line, to split it at the last fitting part of the word.
  // If there is hyphens part of the word, they will be used to
  // cut it properly. If not, the cut will be done using the 
  // Hyphenator component.

  const char *hyphenLoc      = nullptr;
  const char *newHyphenLoc   = nullptr;
  const char *charM0         = nullptr;
  const char *charM1         = nullptr;

  int         passCount = 0;

  bool hyphenate = strlen(word) > 4;

  if (computeMode != ComputeMode::DISPLAY) {

    // Here is the processing done related to the generation of the .locs (page locations) 
    // and .toc (table of content) files. It is a reduced algorithm that does'nt
    // produce a display list but does all the calculation to find where the pages start.

    // This loop advance one usefull character at a time and will stop when
    // all the word's characters were processed, or when the currently processes
    // character would overflow the line and produce a split of the word.

    while (*str) {
      const char *str1, *str2;
      uint32_t    uc1, uc2;

      // charM1 is the last character known to fit on the current line. So it represents
      // the last location into which it is safe to put an hyphen that will not overflow
      // the available line width.

      charM1 = charM0;
      charM0 = str;

      // hyphenLoc is the last hyphen seen in the word that still fit in the current line.

      if (newHyphenLoc) {
        hyphenLoc = newHyphenLoc;
        newHyphenLoc = nullptr;
      }
      newHyphenLoc = (*str == '-') ? str : nullptr;

      while (true) {
        std::tie(uc1, str1) = UTF8::toUnicode(str, fmt.textTransform, first);
        passCount += (str1 - str) - 1;
        if (uc1 != (char32_t)0xad) { break; }
        passCount += 1;
        str = str1;
      }

      while (true) {
        std::tie(uc2, str2) = UTF8::toUnicode(str1, fmt.textTransform, false);
        if (uc2 != (char32_t)0xad) { break; }
        str1 = str2;
      }

      auto [glyph, kern, ignoreNext] = font->getGlyph(uc1, uc2, fmt.fontSize);

      str = ignoreNext ? str2 : str1;

      int16_t advance{ 0 };

      if (glyph == nullptr) {
        glyph = font->getGlyph(' ', fmt.fontSize);
        if (glyph != nullptr) {
          advance = glyph->advance;
        }
      } else {
        advance = glyph->advance + kern;
      }

      if (glyph != nullptr) {
        wordWidth += advance;
        int16_t maxIdx = (charM1 == nullptr) ? -1 : charM1 - word;

        if (!addToEndOfLine) {
          if ((lineWidth + wordWidth > availableWidth) && ((maxIdx <= 1) || !hyphenate)) {
            addLine(fmt, true);
            if (theScreenIsFull(fmt, font)) { return word; }
            hyphenate = true;
          }
          if (lineWidth + wordWidth > availableWidth) {
            if (hyphenLoc != nullptr) {
              std::string tempStr(word, hyphenLoc + 1);
              addWord(tempStr.c_str(), fmt, true);
              return screenIsFull ? hyphenLoc + 1 : addWord(hyphenLoc + 1, fmt);
            } else {
              auto [w, v, k] = UTF8::extractWord(word, maxIdx);
              auto vect = hyphenator->findHyphenIndices(w.c_str(), 1, k);
              if ((vect != nullptr) && !vect->empty() && (k != -1)) {
                uint16_t targetIdx = 0;
                for (auto idx : *vect) {
                  if (idx > k) { break; }
                  targetIdx = idx;
                }
                if (targetIdx != 0) {
                  charM1 = &word[v[targetIdx]];
                  std::string tempStr(word, charM1);
                  tempStr += '-';
                  addWord(tempStr.c_str(), fmt, true);
                  return screenIsFull ? charM1 : addWord(charM1, fmt, false);
                }
                hyphenate = false;
              } else {
                addLine(fmt, true);
                if (theScreenIsFull(fmt, font)) { return word; }
              }
            }
          }
        }

        first = false;
        pageEmpty = false;
      }
    }

    if (addToEndOfLine) {
      if (glyphsHeight < height) { glyphsHeight = height; }
      if (lineHeightFactor < fmt.lineHeightFactor) { lineHeightFactor = fmt.lineHeightFactor; }
      lineWidth += wordWidth;
      addLine(fmt, true);
      theScreenIsFull(fmt, font);
    } else {
      if ((lineWidth + wordWidth) > availableWidth) {
        addLine(fmt, true);
        if (theScreenIsFull(fmt, font)) { return word; }
      }

      if (glyphsHeight < height) { glyphsHeight = height; }
      if (lineHeightFactor < fmt.lineHeightFactor) { lineHeightFactor = fmt.lineHeightFactor; }
      lineWidth += wordWidth;
    }

    return nullptr;

  } // computeMode != ComputeMode::DISPLAY

  // -----------------------

  if (lineList->empty() && (theScreenIsFull(fmt, font))) { 
    return word; 
  }

  auto wordEntries = DisplayList::Make(displayListPool);

  // This loop advance one usefull character at a time and will stop when
  // all the word's characters were processed, or when the currently processes
  // character would overflow the line and produce a split of the word.

  while (*str) {
    const char *str1, *str2;
    char32_t    uc1, uc2;

    // charM1 is the last character known to fit on the current line. So it represents
    // the last location into which it is safe to put an hyphen that will not overflow
    // the available line width.

    charM1 = charM0;
    charM0 = str;

    // hyphenLoc is the last hyphen seen in the word that still fit in the current line.
    
    if (newHyphenLoc) {
      hyphenLoc = newHyphenLoc;
      newHyphenLoc = nullptr;
    }
    newHyphenLoc = (*str == '-') ? str : nullptr;

    // Retrieve the first unicode character. We bypass the presence of U+00AD soft hyphen characters.

    while (true) {
      std::tie(uc1, str1) = UTF8::toUnicode(str, fmt.textTransform, first);
      passCount += (str1 - str) - 1;
      if (uc1 != (char32_t)0xad) { break; }
      passCount += 1;
      str = str1;
    }

    // Retrieve the next unicode character. We bypass the presence of U+00AD soft hyphen characters.
    // Required to compute kerning and ligatures

    while (true) {
      std::tie(uc2, str2) = UTF8::toUnicode(str1, fmt.textTransform, false);
      if (uc2 != (char32_t)0xad) { break; }
      str1 = str2;
    }

    auto [glyph, kern, ignoreNext] = font->getGlyph(uc1, uc2, fmt.fontSize);

    // If a ligature glyph was returned, ignoreNext would be true. If so, we will
    // bypass the second character for the next loop

    str = ignoreNext ? str2 : str1;

    int16_t advance{ 0 };

    // If no glyph was found, insert a U+FFFD or a space to replace it.

    if (glyph == nullptr) {
      glyph = font->getGlyph(0xFFFD, fmt.fontSize);
      if (glyph == nullptr) {
        glyph = font->getGlyph(' ', fmt.fontSize);
      }
      if (glyph != nullptr) {
        advance = glyph->advance;
      }
    } else {
      advance = glyph->advance + kern;
    }

    if (glyph != nullptr) {
      wordWidth += advance;

      if (!addToEndOfLine) {

        // We now need to figure out if the word must be splitted.

        int16_t maxIdx = (charM1 == nullptr) ? -1 : charM1 - word;

        if ((lineWidth + wordWidth > availableWidth) && ((maxIdx <= 1) || !hyphenate)) {
          addLine(fmt, true);
          if (theScreenIsFull(fmt, font)) { return word; }
          hyphenate = true;
        }
        if (lineWidth + wordWidth > availableWidth) {
          // LOG_I("Word too large: {} passCount {}", word, passCount);
          if (hyphenLoc != nullptr) {
            std::string tempStr(word, hyphenLoc + 1);
            addWord(tempStr.c_str(), fmt, true);
            return screenIsFull ? hyphenLoc + 1 : addWord(hyphenLoc + 1, fmt, false);
          } else {
            auto [w, v, k] = UTF8::extractWord(word, maxIdx);
            auto vect = hyphenator->findHyphenIndices(w.c_str(), 1, k);
            if ((vect != nullptr) && !vect->empty() && (k != -1)) {

              // LOG_I("Hyphen positions for {} count {}:", word, vect->size());
              // for (auto val : *vect) {
              //   LOG_I("Position {}", val);
              // }

              uint16_t targetIdx = 0;
              for (auto idx : *vect) {
                if (idx > k) { break; }
                targetIdx = idx;
              }
              if (targetIdx != 0) {
                // LOG_I("TargetIdx = {}", targetIdx);
                charM1 = &word[v[targetIdx]];
                std::string tempStr(word, charM1);
                tempStr += '-';
                addWord(tempStr.c_str(), fmt, true);
                return screenIsFull ? charM1 : addWord(charM1, fmt, false);
              }
            } else {
              addLine(fmt, true);
              if (theScreenIsFull(fmt, font)) { return word; }
            }

            hyphenate = false;
          }
        }
      }

      first = false;
      pageEmpty = false;

      DisplayListEntry *entry = wordEntries->getNewEntry();
      if (entry == nullptr) { return word; }

      entry->command = DisplayListCommand::GLYPH;
      entry->v       = GlyphEntry{ glyph, advance, false };
      entry->pos     = { 0, fmt.verticalAlign };

      wordEntries->pushBack(entry);
    }
  }

  // The received word (or part of a word) was completly processed

  if (addToEndOfLine) {

    // We received a splitted portion of an hyphenated word, known to fit at the 
    // end of current line. It is then merged to the line and a check is done if the end 
    // of the screen was reached.

    lineList->merge(*wordEntries.get());
    if (glyphsHeight < height) { glyphsHeight = height; }
    if (lineHeightFactor < fmt.lineHeightFactor) { lineHeightFactor = fmt.lineHeightFactor; }
    lineWidth += wordWidth;
    addLine(fmt, true);
    theScreenIsFull(fmt, font);
  } else {

    // If the word doesn't fit in the current line, the line is then merged to the page.
    // The addLine is doing so and preparing for a new line to be generated. Then a check
    // to see if the end of the screen was reached. If so, the word is returned to the
    // caller to keep the position for next screen preparation. If not, the word is merged
    // to the current line.

    if ((lineWidth + wordWidth) > availableWidth) {
      addLine(fmt, true);
      if (theScreenIsFull(fmt, font)) { return word; }
    }

    lineList->merge(*wordEntries.get());

    if (glyphsHeight < height) { glyphsHeight = height; }
    if (lineHeightFactor < fmt.lineHeightFactor) { lineHeightFactor = fmt.lineHeightFactor; }
    lineWidth += wordWidth;
  }

  // Returning a nullptr means that the word was properly integrated to a line.

  return nullptr;
}

auto Page::addChar(const char *ch, const Format &fmt) -> bool {
  Glyph *  glyph;

  FontPtr &font = fonts.getFont(fmt.fontIndex);

  if (screenIsFull) { return false; }

  if (lineList->empty() && theScreenIsFull(fmt, font)) { return false; }

  auto [code, s1] = UTF8::toUnicode(ch, fmt.textTransform, true);

  glyph = font->getGlyph(code, fmt.fontSize);

  if (glyph == nullptr) {
    glyph = font->getGlyph(0xFFFD, fmt.fontSize);
    if (glyph == nullptr) {
      glyph = font->getGlyph(' ', fmt.fontSize);
    }
  }

  if (glyph != nullptr) {
    // Verify that there is enough space for the glyph on the line.
    if ((lineWidth + (glyph->advance)) > (paraMaxX - paraMinX - paraIndent)) {
      addLine(fmt, true);
      if (theScreenIsFull(fmt, font)) { return false; }
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
 * - Determines target picture dimensions based on available paragraph space and format
 * constraints
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
    return { false, std::move(picture) };
  }

  // Compute the baseline advance for the bitmap, using info from the current font
  Glyph *     glyph;
  FontPtr &   font = fonts.getFont(fmt.fontIndex);

  const char *str = "m";

  auto [code, s1] = UTF8::toUnicode(str, fmt.textTransform, true);

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
  auto    dim             = picture->getDim();

  if (fmt.width || fmt.height) {
    if (fmt.width && (fmt.width < targetWidth)) { targetWidth = fmt.width; }
    if (fmt.height && (fmt.height < targetHeight)) { targetHeight = fmt.height; }

    w = targetWidth;
    h = dim.height * w / dim.width;
    if (h > targetHeight) {
      h = targetHeight;
      w = dim.width * h / dim.height;
    }
  } else {
    if ((dim.width < targetWidth) && (dim.height < targetHeight)) {
      float scale = std::min(2.0f, std::min(static_cast<float>(targetWidth) / dim.width,
                                            static_cast<float>(targetHeight) / dim.height));
      w           = dim.width * scale;
      h           = dim.height * scale;
      // if (h > targetHeight) {
      //   h = targetHeight;
      //   w = dim.width * h / dim.height;
      // }

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

  if ((lineWidth + advance) > (paraMaxX - paraMinX - paraIndent)) {
    // Not enough room on line, try to put the bitmap on next line
    addLine(fmt, true);
  }

  if ((theScreenIsFull(fmt, font, h))) { return { false, std::move(picture) }; }

  if ((w != dim.width) || (h != dim.height)) {

    // unsigned char * resized_bitmap = nullptr;

    if (computeMode == ComputeMode::DISPLAY) {
      if ((dim.width > 2) || (dim.height > 2)) {

        if ((w == 0) || (h == 0)) { return { false, std::move(picture) }; }

        if ((w != dim.width) || (h != dim.height)) { picture->resize(Dim(w, h)); }
      }
    } else {
      // In non DISPLAY compute mode, we don't want to do the actual resizing of the picture as it
      // is not needed and can be costly. We just set the dimensions that will be used for
      // layouting and rendering the picture later on when in DISPLAY compute mode.
      picture->setDim(Dim(w, h));
    }
  }

  addPictureToLine(std::move(picture), advance, fmt);

  return { true, nullptr };
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
 *       If allocation fails, a message is sent through the logger.
 *       Processing stops if addChar() or addWord() returns false.
 *
 * @see addWord()
 * @see addChar()
 */
auto Page::addText(const std::string &str, const Format &fmt) -> void {
  Format                  myfmt = fmt;

  std::unique_ptr<char[]> buff = std::make_unique<char[]>(100);

  if (buff == nullptr) { LOG_E("addText(): Not enough memory!"); return; }

  const char *s = str.c_str();
  while (*s) {
    if (uint8_t(*s) <= ' ') {
      if (*s == ' ') {
        myfmt.trim = *s == ' ';
        if (!addChar(s, myfmt)) { break; }
      }
      ++s;
    } else {
      int16_t count = 0;
      while ((uint8_t(*s) > ' ') && (count < 99)) {
        buff[count++] = *s++;
      }
      buff[count] = 0;
      if (addWord(buff.get(), myfmt) != nullptr) { break; }
    }
  }
}

auto Page::putPicture(PicturePtr picture, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) { return; }

  if (computeMode == ComputeMode::DISPLAY) {
    entry->v = PictureEntry{ std::move(picture), 0 };
  }
  entry->command = DisplayListCommand::PICTURE;
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("draw_bitmap with a negative location: {} {}", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("draw_bitmap with a too large location: {} {}", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::putHighlight(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) { return; }

  entry->command = DisplayListCommand::HIGHLIGHT;
  entry->v       = RegionEntry{ dim };
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("putHighlight with a negative location: {} {}", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("putHighlight with a too large location: {} {}", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::clearHighlight(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) { return; }

  entry->command = DisplayListCommand::CLEAR_HIGHLIGHT;
  entry->v       = RegionEntry{ dim };
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("Put_str_at with a negative location: {} {}", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("Put_str_at with a too large location: {} {}", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::putRounded(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) { return; }

  entry->command = DisplayListCommand::ROUNDED;
  entry->v       = RegionEntry{ dim };
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("putHighlight with a negative location: {} {}", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("putHighlight with a too large location: {} {}", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::clearRounded(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) { return; }

  entry->command = DisplayListCommand::CLEAR_ROUNDED;
  entry->v       = RegionEntry{ dim };
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("Put_str_at with a negative location: {} {}", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("Put_str_at with a too large location: {} {}", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::clearRegion(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) { return; }

  entry->command = DisplayListCommand::CLEAR_REGION;
  entry->v       = RegionEntry{ dim };
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("Put_str_at with a negative location: {} {}", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("Put_str_at with a too large location: {} {}", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::setRegion(Dim dim, Pos pos) -> void {
  DisplayListEntry *entry = displayList->getNewEntry();
  if (entry == nullptr) { return; }

  entry->command = DisplayListCommand::SET_REGION;
  entry->v       = RegionEntry{ dim };
  entry->pos     = pos;

  #if DEBUGGING
    // if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
    //   LOG_E("Put_str_at with a negative location: {} {}", entry->pos.x, entry->pos.y);
    // }
    // else
    if ((entry->pos.x >= Screen::getWidth()) || (entry->pos.y >= Screen::getHeight())) {
      LOG_E("Put_str_at with a too large location: {} {}", entry->pos.x, entry->pos.y);
    }
  #endif

  displayList->pushBack(entry);
}

auto Page::showCover(PicturePtr &pict) -> bool {
  if (computeMode == ComputeMode::DISPLAY) {
    int32_t picture_width  = pict->getDim().width;
    int32_t picture_height = pict->getDim().height;

    if (pict->getBitmap() != nullptr) {
      // LOG_D("Picture: width: {} height: {} channel_count: {}", picture_width, picture_height,
      // channel_count);

      Dim dim;
      Pos pos;

      dim.width  = Screen::getWidth();
      dim.height = picture_height * Screen::getWidth() / picture_width;

      if (dim.height > Screen::getHeight()) {
        dim.height = Screen::getHeight();
        dim.width  = picture_width * Screen::getHeight() / picture_height;
      }

      pos = { (uint16_t)((Screen::getWidth() - dim.width) >> 1),
              (uint16_t)((Screen::getHeight() - dim.height) >> 1) };

      if ((picture_width != dim.width) || (picture_height != dim.height)) {
        LOG_D("Resizing cover from {}x{} to {}x{}", picture_width, picture_height, dim.width, dim.height);
        pict->resize(dim);
      }

      screen.clear();
      if (pict && pict->getBitmap()) {
        screen.drawPicture(pict, pos);
      } else {
        LOG_W("Unable to load cover file");
        return false;
      }
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
    // LOG_D("getPixelValue(): Str value: {}", value.str);
    return 0;
  default:
    // LOG_D("getPixelValue: Wrong data type!: {}", value.valueType);
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
    LOG_D("getPointValue(): Str value: {}.", value.str);
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
    if (!elementCss->rulesMap.empty()) { adjustFormatFromRules(fmt, elementCss->rulesMap); }
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
    fontStyle = (FaceStyle)vals->front().choice.faceStyle;
  }
  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::FONT_WEIGHT))) {
    fontWeight = (FaceStyle)vals->front().choice.faceStyle;
  }

  FaceStyle newStyle = fonts.adjustFontStyle(fmt.fontStyle, fontStyle, fontWeight);

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::FONT_FAMILY))) {
    int16_t idx = -1;
    for (auto &fontName : *vals) {
      if ((idx = fonts.getFontIndex(fontName.str, newStyle)) != -1) { break; }
    }
    if (idx == -1) {
      LOG_D("Font not found 1: {} {}", vals->front().str, (int)newStyle);
      idx = fonts.getFontIndex("Default", newStyle);
    }
    if (idx == -1) {
      fmt.fontStyle = FaceStyle::NORMAL;
      fmt.fontIndex = 3;
    } else {
      // LOG_D("Font index: {}", idx);
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
    fmt.align = (HAlign)vals->front().choice.hAlign;
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::TEXT_INDENT))) {
    fmt.indent = getPixelValue(vals->front(), fmt, paintWidth());
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::FONT_SIZE))) {
    fmt.fontSize = getPointValue(vals->front(), fmt, fmt.fontSize);
    if (fmt.fontSize == 0) {
      LOG_E("adjustFormatFromSuite: setting fmt.fontSize to 0!!!");
    }
  }

  // if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::LINE_HEIGHT))) {
  //   fmt.lineHeightFactor = getFactorValue(vals->front(), fmt, fmt.lineHeightFactor);
  // }

  int16_t widthRef  = Screen::getWidth() - fmt.screenLeft - fmt.screenRight;
  int16_t heightRef = Screen::getHeight() - fmt.screenTop - fmt.screenBottom;

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN))) {

    int16_t size = 0;
    for (auto val __attribute__((unused)) : *vals) ++size;
    CSS::Values::ConstIterator it = vals->begin();

    if (size == 1) {
      fmt.marginTop = fmt.marginBottom = getPixelValue(vals->front(), fmt, heightRef, true);
      fmt.marginRight = fmt.marginLeft = getPixelValue(vals->front(), fmt, widthRef);
    } else if (size == 2) {
      fmt.marginTop = fmt.marginBottom = getPixelValue(*it++, fmt, heightRef, true);
      fmt.marginRight = fmt.marginLeft = getPixelValue(*it, fmt, widthRef);
    } else if (size == 3) {
      fmt.marginTop   = getPixelValue(*it++, fmt, heightRef, true);
      fmt.marginRight = fmt.marginLeft = getPixelValue(*it++, fmt, widthRef);
      fmt.marginBottom                 = getPixelValue(*it, fmt, heightRef, true);
    } else if (size == 4) {
      fmt.marginTop    = getPixelValue(*it++, fmt, heightRef, true);
      fmt.marginRight  = getPixelValue(*it++, fmt, widthRef);
      fmt.marginBottom = getPixelValue(*it++, fmt, heightRef, true);
      fmt.marginLeft   = getPixelValue(*it, fmt, widthRef);
    }
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::DISPLAY))) {
    fmt.display = vals->front().choice.display;
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN_LEFT))) {
    fmt.marginLeft = getPixelValue(vals->front(), fmt, widthRef);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN_RIGHT))) {
    fmt.marginRight = getPixelValue(vals->front(), fmt, widthRef);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN_TOP))) {
    fmt.marginTop = getPixelValue(vals->front(), fmt, heightRef, true);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::MARGIN_BOTTOM))) {
    fmt.marginBottom = getPixelValue(vals->front(), fmt, heightRef, true);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::WIDTH))) {
    fmt.width = getPixelValue(vals->front(), fmt,
                              fmt.width /*Screen::getWidth() - fmt.screenLeft - fmt.screenRight */);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::HEIGHT))) {
    fmt.height =
      getPixelValue(vals->front(), fmt, heightRef);
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::TEXT_TRANSFORM))) {
    fmt.textTransform = vals->front().choice.textTransform;
  }

  if ((vals = CSS::getValuesFromRules(rules, CSS::PropertyId::VERTICAL_ALIGN))) {
    if (vals->front().choice.vAlign == VAlign::NORMAL) {
      fmt.verticalAlign = 0;
    } else if (vals->front().choice.vAlign == VAlign::SUB) {
      fmt.verticalAlign = 5;
    } else if (vals->front().choice.vAlign == VAlign::SUPER) {
      fmt.verticalAlign = -5;
    } else if (vals->front().choice.vAlign == VAlign::VALUE) {
      FontPtr &font = fonts.getFont(fmt.fontIndex);

      fmt.verticalAlign = -getPixelValue(vals->front(), fmt, font->getLineHeight(fmt.fontSize));
    }
  }
}