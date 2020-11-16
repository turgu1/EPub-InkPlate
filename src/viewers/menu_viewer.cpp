#define __MENU_VIEWER__ 1
#include "viewers/menu_viewer.hpp"

#include "models/fonts.hpp"
#include "models/ttf2.hpp"
#include "viewers/page.hpp"
#include "screen.hpp"

void MenuViewer::show(MenuEntry * the_menu)
{
  TTF * font = fonts.get(0, 16);

  int16_t xpos = 10, ypos = 45;

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   0,
    .font_size          =  16,
    .indent             =   0,
    .margin_left        =   0,
    .margin_right       =   0,
    .margin_top         =   0,
    .margin_bottom      =   0,
    .screen_left        =  10,
    .screen_right       =  10,
    .screen_top         =  10,
    .screen_bottom      = 100,
    .width              =   0,
    .height             =   0,
    .trim               = true,
    .font_style         = Fonts::NORMAL,
    .align              = CSS::LEFT_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM
  };

  page.start(fmt);

  page.clear_region(Screen::WIDTH, fmt.screen_bottom, 0, 0);

  menu = the_menu;

  uint8_t idx = 0;

  while ((idx < MAX_MENU_ENTRY) && (menu[idx].icon != END_MENU)) {

    char ch = icon_char[menu[idx].icon];
    TTF::BitmapGlyph * glyph;
    glyph = font->get_glyph(ch);

    entry_locs[idx].x = xpos;
    entry_locs[idx].y = ypos + glyph->yoff;
    entry_locs[idx].width  = glyph->width;
    entry_locs[idx].height = glyph->rows;

    page.put_char_at(ch, xpos, ypos, fmt);
    xpos += 60;

    idx++;
  }
  
  max_index           = idx - 1;
  current_entry_index = 0;

  page.put_highlight(
    entry_locs[0].width  + 8, 
    entry_locs[0].height + 8, 
    entry_locs[0].x - 4, 
    entry_locs[0].y - 4);

  fmt.font_index = 1;
  fmt.font_size  = 12;
  
  std::string txt = menu[0].caption; 
  page.put_str_at(txt, 10, 80, fmt);

  page.put_highlight(Screen::WIDTH - 20, 3, 10, 85);

  page.paint(false);
}

void MenuViewer::event(EventMgr::KeyEvent key)
{
  uint8_t old_index = current_entry_index;

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   0,
    .font_size          =  16,
    .indent             =   0,
    .margin_left        =   0,
    .margin_right       =   0,
    .margin_top         =   0,
    .margin_bottom      =   0,
    .screen_left        =  10,
    .screen_right       =  10,
    .screen_top         =  10,
    .screen_bottom      =  90,
    .width              =   0,
    .height             =   0,
    .trim               = true,
    .font_style         = Fonts::NORMAL,
    .align              = CSS::LEFT_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM
  };

  page.start(fmt);

  switch (key) {
    case EventMgr::KEY_LEFT:
      if (current_entry_index > 0) {
        current_entry_index--;
      }
      else {
        current_entry_index = max_index;
      }
      break;
    case EventMgr::KEY_RIGHT:
      if (current_entry_index < max_index) {
        current_entry_index++;
      }
      else {
        current_entry_index = 0;
      }
      break;
    case EventMgr::KEY_UP:
      return;
    case EventMgr::KEY_DOWN:
      return;
    case EventMgr::KEY_SELECT:
      if (menu[current_entry_index].func != nullptr) (*menu[current_entry_index].func)();
      return;
    case EventMgr::KEY_HOME:
      return;
  }

  if (current_entry_index != old_index) {
    page.clear_highlight(
      entry_locs[old_index].width  + 8, 
      entry_locs[old_index].height + 8, 
      entry_locs[old_index].x - 4, 
      entry_locs[old_index].y - 4);
      
    page.put_highlight(
      entry_locs[current_entry_index].width  + 8, 
      entry_locs[current_entry_index].height + 8, 
      entry_locs[current_entry_index].x - 4, 
      entry_locs[current_entry_index].y - 4);

    fmt.font_index = 1;
    fmt.font_size  = 12;
    
    page.clear_region(Screen::WIDTH, 30, 0, 55);

    std::string txt = menu[current_entry_index].caption; 
    page.put_str_at(txt, 10, 75, fmt);
  }

  page.paint(false);
}
