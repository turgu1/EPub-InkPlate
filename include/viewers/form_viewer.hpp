// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __FORM_VIEWER_HPP__
#define __FORM_VIEWER_HPP__

#include "global.hpp"
#include "controllers/event_mgr.hpp"

#include <cinttypes>

class FormViewer
{
  private:
    static constexpr uint8_t MAX_FORM_ENTRY   =  10;
    static constexpr uint8_t MAX_CHOICE_ENTRY =  30;
    static constexpr uint8_t FONT_SIZE        =  10;
    static constexpr uint8_t TOP_YPOS         = 100;
    static constexpr uint8_t BOTTOM_YPOS      =  50;

    struct ChoiceLoc {
      int16_t xpos, ypos;
      int16_t width, height;
    } choice_loc[MAX_CHOICE_ENTRY];

    struct EntryLoc {
      int16_t xpos, ypos;
      int16_t width, height;
      int16_t choices_height;
      ChoiceLoc * choices_loc;
      int8_t first_choice_loc_idx;
      int8_t choice_idx;
    } entries_info[MAX_FORM_ENTRY];

    uint8_t entry_count;
    int16_t all_choices_width;
    int16_t last_choices_width;
    int8_t  current_entry_idx;
    bool    entry_selection;
    bool    highlight_selection;

  public:
    enum FormEntryType { HORIZONTAL_CHOICES, VERTICAL_CHOICES };
    struct Choice {
      const char * caption;
      int8_t value;
    };
    typedef Choice * Choices;

    struct FormEntry {
      const char * caption;
      int8_t * value;
      int8_t choice_count;
      Choices choices;
      FormEntryType entry_type;
    };

    typedef FormEntry * FormEntries;

    FormEntries entries;

    void show(FormEntries form_entries, int8_t size, const std::string & bottom_msg);
    bool event(EventMgr::KeyEvent key);
};

#if __FORM_VIEWER__
  FormViewer form_viewer;
#else
  extern FormViewer form_viewer;
#endif

#endif