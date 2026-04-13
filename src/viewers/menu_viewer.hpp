// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "controllers/event_mgr.hpp"
#include "viewers/page.hpp"

#include <functional>

using MenuViewerPtr = himem_unique_ptr<class MenuViewer>;

class MenuViewer {
private:
  static constexpr char const *TAG = "MenuViewer";

  MenuViewer() = default;

  PagePtr page = Page::Make();

public:
  ~MenuViewer() { LOG_I("MenuViewer destructor called"); }

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make() { return make_unique_himem<MenuViewer>(); }

  enum class Icon {
    RETURN, CLR_HISTORY, REFRESH, BOOK, BOOK_LIST, MAIN_PARAMS, FONT_PARAMS, POWEROFF, WIFI, INFO,
        TOC, DEBUG, DELETE, CLOCK, NTP_CLOCK, CALIB, REVERT, END_MENU
  };
  static constexpr char icon_char[17] = {'@', 'T', 'R', 'E', 'F', 'C', 'A', 'Z', 'S',
                                         'I', 'L', 'H', 'K', 'N', 'Y', 'M', 'U'};
  struct MenuEntry {
    Icon icon;
    bool visible;
    bool highlight;
    std::function<void()> func;
    const char *caption;

    template <typename T>
    void bind(T *instance, void (T::*method)()) {
      func = [instance, method]() { (instance->*method)(); };
    }
  };

  void show_caption(std::string caption, Page::Format &fmt);
  void show(MenuEntry *the_menu, uint8_t entry_index = 0, bool clear_screen = false);
  bool event(const EventMgr::Event &event);
  void clear_highlight();

private:
  static constexpr int16_t ICON_SIZE    = 15;
  static constexpr int16_t CAPTION_SIZE = 10;

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    static constexpr int16_t SPACE_BETWEEN_ICONS = 70;
  #else
    static constexpr int16_t SPACE_BETWEEN_ICONS = 50;
  #endif

  uint8_t current_entry_index;
  uint8_t menu_entry_count;
  uint16_t icon_height, text_height, line_height, region_height;
  uint16_t icon_ypos, text_ypos;

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    bool hint_shown;
    uint8_t find_index(uint16_t x, uint16_t y);
  #endif

  struct EntryLoc {
    Pos pos;
    Dim dim;
  };

  using EntryLocPtr = std::unique_ptr<EntryLoc[]>;

  EntryLocPtr entry_locs;

  MenuEntry *menu;
};
