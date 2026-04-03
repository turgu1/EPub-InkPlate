// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MENU_VIEWER__ 1
#include "viewers/menu_viewer.hpp"

#include "controllers/app_controller.hpp"
#include "menu_viewer.hpp"
#include "models/fonts.hpp"
#include "screen.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/screen_bottom.hpp"

static const std::string TOUCH_AND_HOLD_STR = "Touch & hold icon for info. Tap for action.";

void MenuViewer::show_caption(std::string caption, Page::Format &fmt) {
  fmt.font_index = 1;
  fmt.font_size  = CAPTION_SIZE;

  Font *font = fonts.get(1);
  Dim caption_dim;
  font->get_size(caption.c_str(), &caption_dim, CAPTION_SIZE);
  page.put_str_at(caption,
                  Pos{(uint16_t)((Screen::get_width() - caption_dim.width) >> 1), text_ypos}, fmt);
}

void MenuViewer::show(MenuEntry *the_menu, uint8_t entry_index, bool clear_screen) {
  Font *font = fonts.get(1);

  menu_entry_count        = 1;
  int visible_entry_count = 0;

  while (the_menu[menu_entry_count].icon != Icon::END_MENU) {
    if (the_menu[menu_entry_count].visible) {
      visible_entry_count++;
    }
    menu_entry_count++;
  }

  entry_locs = std::make_unique<EntryLoc[]>(menu_entry_count);
  if (!entry_locs) {
    msg_viewer.out_of_memory("Memory allocation failed for menu entry locations!");
    return;
  }

  line_height = font->get_line_height(CAPTION_SIZE);
  text_height = line_height - font->get_descender_height(CAPTION_SIZE);

  font = fonts.get(0);

  if (font == nullptr) {
    LOG_E("Internal error (Drawings Font not available!");
    return;
  }

  Glyph *icon = font->get_glyph('A', ICON_SIZE);

  icon_height = icon == nullptr ? 50 : icon->dim.height;

  int icon_line_height   = icon_height + 30;
  uint16_t icon_ypos     = 30 + icon_height;
  int max_icons_per_line = (Screen::get_width() - 60) / SPACE_BETWEEN_ICONS;

  if ((max_icons_per_line * SPACE_BETWEEN_ICONS + icon->dim.width) < (Screen::get_width() - 60)) {
    max_icons_per_line++;
  }

  int16_t icons_line_count =
      ((visible_entry_count - 1) + max_icons_per_line - 1) / max_icons_per_line;

  text_ypos = icons_line_count * icon_line_height + line_height + 20;

  region_height = text_ypos + icon_line_height + 10;

  Page::Format fmt = {
      .font_index    = 0,
      .font_size     = ICON_SIZE,
      .screen_bottom = 100,
  };

  page.start(fmt);

  page.clear_region(Dim{Screen::get_width(), uint16_t(region_height + 20)}, Pos{0, 0});

  page.put_rounded(Dim(Screen::get_width() - 20, region_height), Pos(10, 10));

  menu = the_menu;

  uint8_t idx = 0;

  Pos pos(30, icon_ypos);

  while (idx < menu_entry_count) {

    if ((idx % max_icons_per_line == 0) && (idx != 0)) {
      pos.x = 30;
      pos.y += icon_line_height;
    }

    if (menu[idx].visible) {
      char ch      = icon_char[(int)menu[idx].icon];
      Glyph *glyph = font->get_glyph(ch, ICON_SIZE);

      if (glyph == nullptr) {
        entry_locs[idx].pos = pos;
        entry_locs[idx].dim = Dim(0, 0);
      } else {
        entry_locs[idx].dim = glyph->dim;
        if (idx < (menu_entry_count - 1)) {
          entry_locs[idx].pos.x = pos.x;
          entry_locs[idx].pos.y = pos.y + glyph->yoff;
        } else {
          entry_locs[idx].pos.x = pos.x = screen.get_width() - 40 - glyph->dim.width;
          entry_locs[idx].pos.y = (pos.y = region_height - glyph->dim.height + 20) + glyph->yoff;
        }
      }

      // page.put_highlight(
      //   Dim(entry_locs[idx].dim.width + 30, entry_locs[idx].pos.y + entry_locs[idx].dim.height +
      //   15), Pos(entry_locs[idx].pos.x - 15, 0));

      page.put_char_at(ch, pos, fmt);
      pos.x += SPACE_BETWEEN_ICONS;

      // std::cout << "["
      //           << entry_locs[idx].pos.x
      //           << ", "
      //           << entry_locs[idx].pos.y
      //           << ":"
      //           << entry_locs[idx].dim.width
      //           << ", "
      //           << entry_locs[idx].dim.height
      //           << "] ";
    }

    idx++;
  }

  // std::cout << std::endl;

  // It is expected that the last entry in the menu will be always visible
  // If not, shit happen...
  while (!menu[entry_index].visible) entry_index++;
  current_entry_index = entry_index;

  #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
    page.put_highlight(
        Dim(entry_locs[entry_index].dim.width + 8, entry_locs[entry_index].dim.height + 8),
        Pos(entry_locs[entry_index].pos.x - 4, entry_locs[entry_index].pos.y - 4));
  #endif

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    show_caption(TOUCH_AND_HOLD_STR, fmt);
    hint_shown = false;
  #else
    show_caption(std::string(menu[entry_index].caption), fmt);
  #endif

  // page.put_highlight(Dim(Screen::get_width() - 20, 3), Pos(10, region_height - 12));

  ScreenBottom::show();

  page.paint(clear_screen);
}

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  uint8_t MenuViewer::find_index(uint16_t x, uint16_t y) {
    LOG_D("Find Index: [%u %u]", x, y);

    // page.put_highlight(Dim(5, 5), Pos(x-2, y-2));
    // page.put_highlight(Dim(7, 7), Pos(x-3, y-3));
    // page.paint(false, true, true);

    for (int8_t idx = 0; idx < menu_entry_count; idx++) {
      if ((x >= entry_locs[idx].pos.x - 15) &&
          (x <= (entry_locs[idx].pos.x + entry_locs[idx].dim.width + 15)) &&
          //(y >=  0) &&
          (y <= (entry_locs[idx].pos.y + entry_locs[idx].dim.height + 15))) {
        return idx;
      }
    }

    return menu_entry_count;
  }
#endif

void MenuViewer::clear_highlight() {
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    Page::Format fmt = {
        .font_size     = CAPTION_SIZE,
        .screen_bottom = 0,
    };

    page.start(fmt);

    if (hint_shown) {
      hint_shown = false;

      page.clear_highlight(Dim(entry_locs[current_entry_index].dim.width + 8,
                               entry_locs[current_entry_index].dim.height + 8),
                           Pos(entry_locs[current_entry_index].pos.x - 4,
                               entry_locs[current_entry_index].pos.y - 4));

      page.clear_region(Dim(Screen::get_width() - 22, text_height),
                        Pos(11, text_ypos - line_height));
      show_caption(TOUCH_AND_HOLD_STR, fmt);
    }

    page.paint(false);
  #endif
}

bool MenuViewer::event(const EventMgr::Event &event) {
  Page::Format fmt = {
      .font_size     = CAPTION_SIZE,
      .screen_bottom = 0,
  };

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL

    switch (event.kind) {
    case EventMgr::EventKind::HOLD:
      current_entry_index = find_index(event.x, event.y);
      if (current_entry_index < menu_entry_count) {
        page.start(fmt);

        page.clear_region(Dim(Screen::get_width() - 22, text_height),
                          Pos(11, text_ypos - line_height));

        show_caption(std::string(menu[current_entry_index].caption), fmt);
        hint_shown = true;

        page.paint(false);
      }
      break;

    case EventMgr::EventKind::RELEASE:
      #if EPUB_INKPLATE_BUILD
        ESP::delay(1000);
      #endif
      clear_highlight();
      hint_shown = false;
      break;

    case EventMgr::EventKind::TAP:
      current_entry_index = find_index(event.x, event.y);
      if (current_entry_index < menu_entry_count) {
        if (menu[current_entry_index].func != nullptr) {
          if (menu[current_entry_index].highlight) {
            page.start(fmt);

            page.clear_region(Dim(Screen::get_width() - 22, text_height),
                              Pos(11, text_ypos - line_height));

            show_caption(std::string(menu[current_entry_index].caption), fmt);
            hint_shown = true;

            page.put_highlight(Dim(entry_locs[current_entry_index].dim.width + 8,
                                   entry_locs[current_entry_index].dim.height + 8),
                               Pos(entry_locs[current_entry_index].pos.x - 4,
                                   entry_locs[current_entry_index].pos.y - 4));

            page.paint(false);
          } else {
            hint_shown = false;
          }

          (*menu[current_entry_index].func)();
        }
        return false;
      }
      break;

    default:
      break;
    }
  #else // Non touch devices
    uint8_t old_index = current_entry_index;

    page.start(fmt);

    switch (event.kind) {
    case EventMgr::EventKind::PREV:
      if (current_entry_index > 0) {
        current_entry_index--;
        // It is expected that the first entry in the menu will always be visible
        while (!menu[current_entry_index].visible) current_entry_index--;
      } else {
        current_entry_index = menu_entry_count - 1;
      }
      break;
    case EventMgr::EventKind::NEXT:
      if (current_entry_index < menu_entry_count - 1) {
        current_entry_index++;
        // It is expected that the last entry in the menu will always be visible
        while (!menu[current_entry_index].visible) current_entry_index++;
      } else {
        current_entry_index = 0;
      }
      break;
    case EventMgr::EventKind::DBL_PREV:
      return false;
    case EventMgr::EventKind::DBL_NEXT:
      return false;
    case EventMgr::EventKind::SELECT:
      if (menu[current_entry_index].func != nullptr) (*menu[current_entry_index].func)();
      return false;
    case EventMgr::EventKind::DBL_SELECT:
      return true;
    case EventMgr::EventKind::NONE:
      return false;
    }

    if (current_entry_index != old_index) {
      page.clear_highlight(
          Dim(entry_locs[old_index].dim.width + 8, entry_locs[old_index].dim.height + 8),
          Pos(entry_locs[old_index].pos.x - 4, entry_locs[old_index].pos.y - 4));

      page.put_highlight(Dim(entry_locs[current_entry_index].dim.width + 8,
                             entry_locs[current_entry_index].dim.height + 8),
                         Pos(entry_locs[current_entry_index].pos.x - 4,
                             entry_locs[current_entry_index].pos.y - 4));

      page.clear_region(Dim(Screen::get_width() - 22, text_height),
                        Pos(11, text_ypos - line_height));

      show_caption(std::string(menu[current_entry_index].caption), fmt);
    }

    ScreenBottom::show();

    page.paint(false);
  #endif

  return false;
}
