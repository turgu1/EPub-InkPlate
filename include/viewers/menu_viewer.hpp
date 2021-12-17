// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"

class MenuViewer
{
  public:
    static constexpr uint8_t MAX_MENU_ENTRY = 10;

    enum class Icon { RETURN, REVERT, REFRESH, BOOK, BOOK_LIST, MAIN_PARAMS, 
                      FONT_PARAMS, POWEROFF, WIFI, INFO, TOC, DEBUG, DELETE, CALIB, END_MENU };
    char icon_char[14] = { '@', 'Q', 'R', 'E', 'F', 'C', 'A', 'Z', 'S', 'I', 'L', 'H', 'K', 'M' };
    struct MenuEntry {
      Icon icon;
      const char * caption;
      void (*func)();
      bool visible;
      bool highlight;
    };
    void  show(MenuEntry * the_menu, uint8_t entry_index = 0, bool clear_screen = false);
    bool event(const EventMgr::Event & event);
    void clear_highlight();
    
  private:
    static constexpr char const * TAG = "MenuViewer";

    static const int16_t ICON_SIZE           = 15;
    static const int16_t CAPTION_SIZE        = 12;

    #if INKPLATE_6PLUS
      static const int16_t SPACE_BETWEEN_ICONS = 70;
      static const int16_t ICONS_LEFT_OFFSET   = 20;
    #else
      static const int16_t SPACE_BETWEEN_ICONS = 50;
      static const int16_t ICONS_LEFT_OFFSET   = 10;
    #endif

    uint8_t  current_entry_index;
    uint8_t  max_index;
    uint16_t icon_height, 
             text_height, 
             line_height,
             region_height;
    int16_t  icon_ypos,
             text_ypos;

    #if (INKPLATE_6PLUS || TOUCH_TRIAL)
      bool    hint_shown;
      uint8_t find_index(uint16_t x, uint16_t y);
    #endif

    struct EntryLoc {
      Pos pos;
      Dim dim;
    } entry_locs[MAX_MENU_ENTRY];
    MenuEntry * menu;
};

#if __MENU_VIEWER__
  MenuViewer menu_viewer;
#else
  extern MenuViewer menu_viewer;
#endif
