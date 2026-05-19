// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MENU_VIEWER__ 1
#include "viewers/menu_viewer.hpp"

#include "controllers/app_controller.hpp"
#include "fonts.hpp"
#include "menu_viewer.hpp"
#include "screen.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/screen_bottom.hpp"

#include <algorithm>

static const std::string TOUCH_AND_HOLD_STR = "Touch & hold icon for info. Tap for action.";

auto MenuViewer::showCaption(std::string caption, Page::Format &fmt) -> void {
  fmt.fontIndex = SYSTEM_REGULAR_FONT_INDEX;
  fmt.fontSize  = CAPTION_SIZE;

  FontPtr &font = appFonts.getFont(1);
  Dim captionDim;
  font->getASCIISize(caption.c_str(), &captionDim, CAPTION_SIZE);
  page->putStrAt(caption, Pos{(uint16_t)((Screen::getWidth() - captionDim.width) >> 1), textYPos},
                 fmt);
}

auto MenuViewer::show(MenuEntry *theMenu, uint8_t entryIndex, bool clearScreen) -> void {
  FontPtr &textFont = appFonts.getFont(1);

  uint16_t iconHeight{0};
  uint16_t iconWidth{0};
  uint16_t spaceBetweenIcons{0};

  menuEntryCount    = 0;
  visibleEntryCount = 0;

  while (theMenu[menuEntryCount].icon != Icon::END_MENU) {
    if (theMenu[menuEntryCount].visible) {
      visibleEntryCount++;
    }
    menuEntryCount++;
  }

  entryLocs = std::make_unique<EntryLoc[]>(menuEntryCount);
  if (!entryLocs) {
    MsgViewer::outOfMemory("Memory allocation failed for menu entry locations!");
    return;
  }

  lineHeight = textFont->getLineHeight(CAPTION_SIZE);
  textHeight = lineHeight - textFont->getDescenderHeight(CAPTION_SIZE);

  FontPtr &iconFont = appFonts.getFont(0);

  Glyph *icon = iconFont->getGlyph('A', ICON_SIZE);

  iconHeight = icon == nullptr ? 60 : icon->dim.height;
  iconWidth  = icon == nullptr ? 60 : icon->dim.width;

  spaceBetweenIcons = iconWidth + (iconWidth >> 1);

  uint16_t iconLineHeight  = iconHeight + 30;
  uint16_t iconYPos        = 30 + iconHeight;
  uint16_t maxIconsPerLine = (Screen::getWidth() - 100) / spaceBetweenIcons;

  if ((maxIconsPerLine * spaceBetweenIcons + iconWidth) < (Screen::getWidth() - 100)) {
    maxIconsPerLine++;
  }

  // If the number of visible entries is one more than the maximum number of
  // icons per line, we need to reduce the maximum number of icons per line by
  // one to avoid having a single icon in the last line.
  if ((visibleEntryCount % maxIconsPerLine) == 2) {
    maxIconsPerLine--;
  }

  uint16_t iconsLineCount = (visibleEntryCount + maxIconsPerLine - 1) / maxIconsPerLine;

  textYPos = iconsLineCount * iconLineHeight + lineHeight + 20;

  regionHeight = textYPos + iconLineHeight + 15;

  // LOG_I("MenuViewer::show: menuEntryCount=%u, visibleEntryCount=%d, iconWidth=%d, "
  //       "spaceBetweenIcons=%d, iconLineHeight=%d, maxIconsPerLine=%d, iconsLineCount=%d, "
  //       "textYPos=%u, regionHeight=%u",
  //       menuEntryCount, visibleEntryCount, iconWidth, spaceBetweenIcons, iconLineHeight,
  //       maxIconsPerLine, iconsLineCount, textYPos, regionHeight);

  Page::Format fmt = {
      .fontIndex    = ICONS_FONT_INDEX,
      .fontSize     = ICON_SIZE,
      .screenBottom = 100,
  };

  page->start(fmt);

  page->clearRegion(Dim{Screen::getWidth(), uint16_t(regionHeight + 20)}, Pos{0, 0});

  page->putRounded(Dim(Screen::getWidth() - 20, regionHeight), Pos(10, 10));

  menu = theMenu;

  // The last icon is placed at a fixed corner position, not in the grid.
  // Use (visibleEntryCount - 1) for all grid centering calculations.
  uint16_t firstLineIconCount =
      std::min(uint16_t(visibleEntryCount - 1), (uint16_t)maxIconsPerLine);
  uint16_t leftMargin = (Screen::getWidth() - (firstLineIconCount * spaceBetweenIcons -
                                               (spaceBetweenIcons - iconWidth))) >>
                        1;
  uint8_t idx        = 0;
  uint8_t visibleIdx = 0;

  Pos pos(leftMargin, iconYPos);
  uint16_t reminderCount = visibleEntryCount;

  while (idx < menuEntryCount) {

    if (menu[idx].visible) {
      if ((visibleIdx % maxIconsPerLine == 0) && (visibleIdx != 0)) {
        // The last icon is placed at a fixed corner position, not in the grid.
        // Count only the remaining grid icons (excluding the corner one).
        reminderCount = (visibleEntryCount - 1) - visibleIdx;
        leftMargin    = (Screen::getWidth() -
                      (std::min(reminderCount, (uint16_t)maxIconsPerLine) * spaceBetweenIcons -
                       (spaceBetweenIcons - iconWidth))) >>
                     1;
        pos.x = leftMargin;
        pos.y += iconLineHeight;
      }

      char ch      = iconChar[(int)menu[idx].icon];
      Glyph *glyph = iconFont->getGlyph(ch, ICON_SIZE);

      if (glyph == nullptr) {
        entryLocs[idx].pos = pos;
        entryLocs[idx].dim = Dim(0, 0);
      } else {
        entryLocs[idx].dim = glyph->dim;
        if (visibleIdx < (visibleEntryCount - 1)) {
          entryLocs[idx].pos.x = pos.x;
          entryLocs[idx].pos.y = pos.y + glyph->yoff;
        } else {
          entryLocs[idx].pos.x = pos.x = screen.getWidth() - 40 - glyph->dim.width;
          entryLocs[idx].pos.y = (pos.y = regionHeight - glyph->dim.height + 20) + glyph->yoff;
        }
      }

      page->putCharAt(ch, pos, fmt);
      pos.x += spaceBetweenIcons;

      visibleIdx++;
    }

    idx++;
  }

  // std::cout << std::endl;

  // It is expected that the last entry in the menu will be always visible
  // If not, shit happen...
  while (!menu[entryIndex].visible) entryIndex++;
  currentEntryIndex = entryIndex;

#if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
  page->putHighlight(Dim(entryLocs[entryIndex].dim.width + 8, entryLocs[entryIndex].dim.height + 8),
                     Pos(entryLocs[entryIndex].pos.x - 4, entryLocs[entryIndex].pos.y - 4));
#endif

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  showCaption(TOUCH_AND_HOLD_STR, fmt);
  hintShown = false;
#else
  showCaption(std::string(menu[entryIndex].caption), fmt);
#endif

  // page->putHighlight(Dim(Screen::getWidth() - 20, 3), Pos(10, regionHeight - 12));

  ScreenBottom::show(page);

  page->paint(clearScreen);
}

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
auto MenuViewer::findIndex(uint16_t x, uint16_t y) -> uint8_t {
  LOG_D("Find Index: [%u %u]", x, y);

  // page->putHighlight(Dim(5, 5), Pos(x-2, y-2));
  // page->putHighlight(Dim(7, 7), Pos(x-3, y-3));
  // page->paint(false, true, true);

  for (int8_t idx = 0; idx < menuEntryCount; idx++) {
    if ((x >= entryLocs[idx].pos.x - 15) &&
        (x <= (entryLocs[idx].pos.x + entryLocs[idx].dim.width + 15)) &&
        //(y >=  0) &&
        (y <= (entryLocs[idx].pos.y + entryLocs[idx].dim.height + 15))) {
      return idx;
    }
  }

  return menuEntryCount;
}
#endif

auto MenuViewer::clearHighlight() -> void {
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  Page::Format fmt = {
      .fontSize     = CAPTION_SIZE,
      .screenBottom = 0,
  };

  page->start(fmt);

  if (hintShown) {
    hintShown = false;

    page->clearHighlight(
        Dim(entryLocs[currentEntryIndex].dim.width + 8,
            entryLocs[currentEntryIndex].dim.height + 8),
        Pos(entryLocs[currentEntryIndex].pos.x - 4, entryLocs[currentEntryIndex].pos.y - 4));

    page->clearRegion(Dim(Screen::getWidth() - 22, textHeight), Pos(11, textYPos - lineHeight));
    showCaption(TOUCH_AND_HOLD_STR, fmt);
  }

  page->paint(false);
#endif
}

auto MenuViewer::event(const EventMgr::Event &event) -> bool {
  Page::Format fmt = {
      .fontSize     = CAPTION_SIZE,
      .screenBottom = 0,
  };

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL

  switch (event.kind) {
  case EventMgr::EventKind::HOLD:
    currentEntryIndex = findIndex(event.x, event.y);
    if (currentEntryIndex < menuEntryCount) {
      page->start(fmt);

      page->clearRegion(Dim(Screen::getWidth() - 22, textHeight), Pos(11, textYPos - lineHeight));

      showCaption(std::string(menu[currentEntryIndex].caption), fmt);
      hintShown = true;

      page->paint(false);
    }
    break;

  case EventMgr::EventKind::RELEASE:
  #if EPUB_INKPLATE_BUILD
    ESP::delay(1000);
  #endif
    clearHighlight();
    hintShown = false;
    break;

  case EventMgr::EventKind::TAP:
    currentEntryIndex = findIndex(event.x, event.y);
    if (currentEntryIndex < menuEntryCount) {
      if (menu[currentEntryIndex].func != nullptr) {
        if (menu[currentEntryIndex].highlight) {
          page->start(fmt);

          page->clearRegion(Dim(Screen::getWidth() - 22, textHeight),
                            Pos(11, textYPos - lineHeight));

          showCaption(std::string(menu[currentEntryIndex].caption), fmt);
          hintShown = true;

          page->putHighlight(
              Dim(entryLocs[currentEntryIndex].dim.width + 8,
                  entryLocs[currentEntryIndex].dim.height + 8),
              Pos(entryLocs[currentEntryIndex].pos.x - 4, entryLocs[currentEntryIndex].pos.y - 4));

          page->paint(false);
        } else {
          hintShown = false;
        }

        (menu[currentEntryIndex].func)();
      }
      return false;
    } else if (regionHeight < event.y) {
      return true;
    }
    break;

  default:
    break;
  }
#else // Non touch devices
  uint8_t oldIndex = currentEntryIndex;

  page->start(fmt);

  switch (event.kind) {
  case EventMgr::EventKind::PREV:
    if (currentEntryIndex > 0) {
      currentEntryIndex--;
      // It is expected that the first entry in the menu will always be visible
      while (!menu[currentEntryIndex].visible) currentEntryIndex--;
    } else {
      currentEntryIndex = menuEntryCount - 1;
    }
    break;
  case EventMgr::EventKind::NEXT:
    if (currentEntryIndex < menuEntryCount - 1) {
      currentEntryIndex++;
      // It is expected that the last entry in the menu will always be visible
      while (!menu[currentEntryIndex].visible) currentEntryIndex++;
    } else {
      currentEntryIndex = 0;
    }
    break;
  case EventMgr::EventKind::DBL_PREV:
    return false;
  case EventMgr::EventKind::DBL_NEXT:
    return false;
  case EventMgr::EventKind::SELECT:
    if (menu[currentEntryIndex].func != nullptr) (menu[currentEntryIndex].func)();
    return false;
  case EventMgr::EventKind::DBL_SELECT:
    return true;
  case EventMgr::EventKind::NONE:
    return false;
  }

  if (currentEntryIndex != oldIndex) {
    page->clearHighlight(Dim(entryLocs[oldIndex].dim.width + 8, entryLocs[oldIndex].dim.height + 8),
                         Pos(entryLocs[oldIndex].pos.x - 4, entryLocs[oldIndex].pos.y - 4));

    page->putHighlight(
        Dim(entryLocs[currentEntryIndex].dim.width + 8,
            entryLocs[currentEntryIndex].dim.height + 8),
        Pos(entryLocs[currentEntryIndex].pos.x - 4, entryLocs[currentEntryIndex].pos.y - 4));

    page->clearRegion(Dim(Screen::getWidth() - 22, textHeight), Pos(11, textYPos - lineHeight));

    showCaption(std::string(menu[currentEntryIndex].caption), fmt);
  }

  ScreenBottom::show(page);

  page->paint(false);
#endif

  return false;
}
