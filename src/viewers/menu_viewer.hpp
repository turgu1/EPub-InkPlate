// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "controllers/event_mgr.hpp"
#include "viewers/page.hpp"

#include <functional>

using MenuViewerPtr = himemUniquePtr<class MenuViewer>;

class MenuViewer {
private:
  static constexpr char const *TAG = "MenuViewer";

  MenuViewer() = default;

  PagePtr page = Page::Make();

public:
  ~MenuViewer() { LOG_I("MenuViewer destructor called"); }

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> himemUniquePtr<T>;

  static inline auto Make() { return makeUniqueHimem<MenuViewer>(); }

  enum class Icon {
    RETURN, CLR_HISTORY, REFRESH, BOOK, BOOK_LIST, MAIN_PARAMS, FONT_PARAMS, POWEROFF, WIFI, INFO,
        TOC, DEBUG, DELETE, CLOCK, NTP_CLOCK, CALIB, REVERT, END_MENU
  };
  static constexpr char iconChar[17] = {'@', 'T', 'R', 'E', 'F', 'C', 'A', 'Z', 'S',
                                        'I', 'L', 'H', 'K', 'N', 'Y', 'M', 'U'};
  struct MenuEntry {
    Icon icon;
    bool visible;
    bool highlight;
    std::function<void()> func;
    const char *caption;

    template <typename T>
    auto bind(T *instance, void (T::*method)()) -> void {
      func = [instance, method]() -> auto { (instance->*method)(); };
    }
  };

  auto showCaption(std::string caption, Page::Format &fmt) -> void;
  auto show(MenuEntry *theMenu, uint8_t entryIndex = 0, bool clearScreen = false) -> void;
  auto event(const EventMgr::Event &event) -> bool;
  auto clearHighlight() -> void;

private:
  static constexpr int16_t ICON_SIZE    = 15;
  static constexpr int16_t CAPTION_SIZE = 10;

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    static constexpr int16_t SPACE_BETWEEN_ICONS = 70;
  #else
    static constexpr int16_t SPACE_BETWEEN_ICONS = 50;
  #endif

  uint8_t currentEntryIndex;
  uint8_t menuEntryCount;
  uint16_t iconHeight, textHeight, lineHeight, regionHeight;
  uint16_t iconYPos, textYPos;

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    bool hintShown;
    auto findIndex(uint16_t x, uint16_t y) -> uint8_t;
  #endif

  struct EntryLoc {
    Pos pos;
    Dim dim;
  };

  using EntryLocPtr = std::unique_ptr<EntryLoc[]>;

  EntryLocPtr entryLocs;

  MenuEntry *menu;
};
