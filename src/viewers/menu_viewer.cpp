// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MENU_VIEWER__ 1
#include "viewers/menu_viewer.hpp"

#include "viewers/book_viewer.hpp"
#include "models/fonts.hpp"
#include "viewers/page.hpp"
#include "viewers/screen_bottom.hpp"
#include "screen.hpp"
#include "controllers/app_controller.hpp"

static const std::string TOUCH_AND_HOLD_STR = "Touch and hold icon for info. Tap for action.";

void MenuViewer::show(MenuEntry * the_menu, uint8_t entry_index, bool clear_screen)
{
  Font * font = fonts.get(1);

  if (font == nullptr) {
    LOG_E("Internal error (Main Font not available!");
    return;
  }

  line_height = font->get_line_height(CAPTION_SIZE);
  text_height = line_height - font->get_descender_height(CAPTION_SIZE); 

  font = fonts.get(0);

  if (font == nullptr) {
    LOG_E("Internal error (Drawings Font not available!");
    return;
  }

  Font::Glyph * icon = font->get_glyph('A', ICON_SIZE);

  if (icon == nullptr) {
    icon_height   = 50;
    icon_ypos     = 10 + icon_height;
    text_ypos     = icon_ypos + line_height + 10;
  }
  else {
    icon_height   = icon->dim.height;
    icon_ypos     = 10 + icon_height;
    text_ypos     = icon_ypos + line_height + 10;
  }

  region_height = text_ypos + 20;

  Page::Format fmt = {
    .line_height_factor =   1.0,
    .font_index         =     0,
    .font_size          = ICON_SIZE,
    .indent             =     0,
    .margin_left        =     0,
    .margin_right       =     0,
    .margin_top         =     0,
    .margin_bottom      =     0,
    .screen_left        =    10,
    .screen_right       =    10,
    .screen_top         =    10,
    .screen_bottom      =   100,
    .width              =     0,
    .height             =     0,
    .vertical_align     =     0,
    .trim               =  true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = CSS::Align::LEFT,
    .text_transform     = CSS::TextTransform::NONE,
    .display            = CSS::Display::INLINE
  };

  page.start(fmt);

  page.clear_region(Dim{ Screen::get_width(), region_height }, Pos{ 0, 0 });

  menu = the_menu;

  uint8_t idx = 0;

  Pos pos(ICONS_LEFT_OFFSET, icon_ypos);
  
  while ((idx < MAX_MENU_ENTRY) && (menu[idx].icon != Icon::END_MENU)) {

    if (menu[idx].visible) {
      char ch = icon_char[(int)menu[idx].icon];
      Font::Glyph * glyph;
      glyph = font->get_glyph(ch, ICON_SIZE);

      if (menu[idx].icon == Icon::NEXT_MENU) pos.x = Screen::get_width() - SPACE_BETWEEN_ICONS;

      if (glyph == nullptr) {
        entry_locs[idx].pos = pos;
        entry_locs[idx].dim = Dim(0, 0);
      }
      else {
        entry_locs[idx].pos.x = pos.x;
        entry_locs[idx].pos.y = pos.y + glyph->yoff;
        entry_locs[idx].dim   = glyph->dim;
      }
      // page.put_highlight(
      //   Dim(entry_locs[idx].dim.width + 30, entry_locs[idx].pos.y + entry_locs[idx].dim.height + 15), 
      //   Pos(entry_locs[idx].pos.x - 15, 0));

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
    else {
      entry_locs[idx].pos.x = -1;
      entry_locs[idx].pos.y = -1;
    }

    idx++;
  }

  // std::cout << std::endl;
  
  max_index           = idx - 1;
  // It is expected that the last entry in the menu will be always visible
  // If not, shit happen...
  while (!menu[entry_index].visible) entry_index++;
  current_entry_index = entry_index;

  #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
    page.put_highlight(
      Dim(entry_locs[entry_index].dim.width  + 8, entry_locs[entry_index].dim.height + 8), 
      Pos(entry_locs[entry_index].pos.x      - 4, entry_locs[entry_index].pos.y - 4));
  #endif

  fmt.font_index = 1;
  fmt.font_size  = CAPTION_SIZE;
  
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    page.put_str_at(TOUCH_AND_HOLD_STR, Pos{ 10, text_ypos }, fmt);
    hint_shown = false;
  #else
    std::string txt = menu[entry_index].caption; 
    page.put_str_at(txt, Pos{ 10, text_ypos }, fmt);
  #endif

  page.put_highlight(
    Dim(Screen::get_width() - 20, 3), 
    Pos(10, region_height - 12));

  ScreenBottom::show();

  page.paint(clear_screen);
}

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  uint8_t
  MenuViewer::find_index(uint16_t x, uint16_t y)
  {
    LOG_D("Find Index: [%u %u]", x, y);
    
    // page.put_highlight(Dim(5, 5), Pos(x-2, y-2));
    // page.put_highlight(Dim(7, 7), Pos(x-3, y-3));
    // page.paint(false, true, true);

    for (int8_t idx = 0; idx <= max_index; idx++) {
      if ((x >=  entry_locs[idx].pos.x - 15) &&
          (x <= (entry_locs[idx].pos.x + entry_locs[idx].dim.width + 15)) &&
          //(y >=  0) &&
          (y <= (entry_locs[idx].pos.y + entry_locs[idx].dim.height + 15))) {
        return idx;
      }
    }

    return max_index + 1;
  }
#endif

void 
MenuViewer::clear_highlight()
{
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    Page::Format fmt = {
      .line_height_factor =   1.0,
      .font_index         =     1,
      .font_size          = CAPTION_SIZE,
      .indent             =     0,
      .margin_left        =     0,
      .margin_right       =     0,
      .margin_top         =     0,
      .margin_bottom      =     0,
      .screen_left        =    10,
      .screen_right       =    10,
      .screen_top         =    10,
      .screen_bottom      =     0,
      .width              =     0,
      .height             =     0,
      .vertical_align     =     0,
      .trim               =  true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

    page.start(fmt);

    if (hint_shown) {
      hint_shown     = false;

      page.clear_highlight(
        Dim(entry_locs[current_entry_index].dim.width + 8, entry_locs[current_entry_index].dim.height + 8), 
        Pos(entry_locs[current_entry_index].pos.x - 4,     entry_locs[current_entry_index].pos.y - 4     ));

      page.clear_region(Dim(Screen::get_width(), text_height), Pos(0, text_ypos - line_height));
      page.put_str_at(TOUCH_AND_HOLD_STR, Pos{ 10, text_ypos }, fmt);
    }

    page.paint(false);
  #endif
}

bool 
MenuViewer::event(const EventMgr::Event & event)
{
  Page::Format fmt = {
    .line_height_factor =   1.0,
    .font_index         =     1,
    .font_size          = CAPTION_SIZE,
    .indent             =     0,
    .margin_left        =     0,
    .margin_right       =     0,
    .margin_top         =     0,
    .margin_bottom      =     0,
    .screen_left        =    10,
    .screen_right       =    10,
    .screen_top         =    10,
    .screen_bottom      =     0,
    .width              =     0,
    .height             =     0,
    .vertical_align     =     0,
    .trim               =  true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = CSS::Align::LEFT,
    .text_transform     = CSS::TextTransform::NONE,
    .display            = CSS::Display::INLINE
  };

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL

    switch (event.kind) {
      case EventMgr::EventKind::HOLD:
        current_entry_index = find_index(event.x, event.y);
        if (current_entry_index <= max_index) {
          page.start(fmt);

          fmt.font_index =  1;
          fmt.font_size  = CAPTION_SIZE;
        
          page.clear_region(Dim(Screen::get_width(), text_height), Pos(0, text_ypos - line_height));

          std::string txt = menu[current_entry_index].caption; 
          page.put_str_at(txt, Pos{ 10, text_ypos }, fmt);
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
        if (current_entry_index <= max_index) {
          if (menu[current_entry_index].func != nullptr) {
            if (menu[current_entry_index].highlight) {
              page.start(fmt);

              fmt.font_index = 1;
              fmt.font_size  = CAPTION_SIZE;
            
              page.clear_region(Dim(Screen::get_width(), text_height), Pos(0, text_ypos - line_height));

              std::string txt = menu[current_entry_index].caption; 
              page.put_str_at(txt, Pos{ 10, text_ypos }, fmt);
              hint_shown = true;

              page.put_highlight(
                Dim(entry_locs[current_entry_index].dim.width + 8, entry_locs[current_entry_index].dim.height + 8),
                Pos(entry_locs[current_entry_index].pos.x - 4,     entry_locs[current_entry_index].pos.y - 4     ));

              page.paint(false);
            }
            else {
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
  #else
    uint8_t old_index = current_entry_index;

    page.start(fmt);

    switch (event.kind) {
      case EventMgr::EventKind::PREV:
        if (current_entry_index > 0) {
          current_entry_index--;
          // It is expected that the first entry in the menu will always be visible
          while (!menu[current_entry_index].visible) current_entry_index--;
        }
        else {
          current_entry_index = max_index;
        }
        break;
      case EventMgr::EventKind::NEXT:
        if (current_entry_index < max_index) {
          current_entry_index++;
          // It is expected that the last entry in the menu will always be visible
          while (!menu[current_entry_index].visible) current_entry_index++;
        }
        else {
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
        Pos(entry_locs[old_index].pos.x - 4,     entry_locs[old_index].pos.y - 4     ));
        
      page.put_highlight(
        Dim(entry_locs[current_entry_index].dim.width  + 8, entry_locs[current_entry_index].dim.height + 8),
        Pos(entry_locs[current_entry_index].pos.x - 4,      entry_locs[current_entry_index].pos.y - 4     ));

      fmt.font_index = 1;
      fmt.font_size  = CAPTION_SIZE;
    
      page.clear_region(Dim(Screen::get_width(), text_height), Pos(0, text_ypos - line_height));

      std::string txt = menu[current_entry_index].caption; 
      page.put_str_at(txt, Pos{ 10, text_ypos }, fmt);
    }

    ScreenBottom::show();

    page.paint(false);
  #endif
  
  return false;
}
