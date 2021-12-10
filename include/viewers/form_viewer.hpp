// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"

class FormViewer
{
  public:
    struct Choice {
      const char   * caption;
      int8_t         value;
    };
    typedef Choice * Choices;

    static constexpr Choice done_choices[1] = {
      { "DONE",   1 }
    };

    static constexpr Choice dir_view_choices[2] = {
      { "LINEAR",     0 },
      { "MATRIX",     1 }
    };

    static constexpr Choice ok_cancel_choices[2] = {
      { "OK",     1 },
      { "CANCEL", 0 }
    };

    static constexpr Choice yes_no_choices[2] = {
      { "YES", 1 },
      { "NO",  0 }
    };

    static constexpr Choice resolution_choices[2] = {
      { "1Bit",  0 },
      { "3Bits", 1 }
    };

    static constexpr Choice timeout_choices[3] = {
      { "5",   5 },
      { "15", 15 },
      { "30", 30 }
    };

    static constexpr Choice battery_visual[4] = {
      { "NONE",    0 },
      { "PERCENT", 1 },
      { "VOLTAGE", 2 },
      { "ICON",    3 }
    };

    static constexpr Choice font_size_choices[4] = {
      { "8",   8 },
      { "10", 10 },
      { "12", 12 },
      { "15", 15 }
    };

    #if (INKPLATE_6PLUS || TOUCH_TRIAL)
      static constexpr Choice orientation_choices[4] = {
        { "LEFT",   3 },
        { "RIGHT",  2 },
        { "TOP",    1 },
        { "BOTTOM", 0 }
      };
    #else
      static constexpr Choice orientation_choices[3] = {
        { "LEFT",   0 },
        { "RIGHT",  1 },
        { "BOTTOM", 2 }
      };
    #endif

    static Choice font_choices[8];
    uint8_t font_choices_count;

    void adjust_font_choices(char ** font_names, uint8_t size);

    inline uint8_t get_font_choices_count() { return font_choices_count; }

  private:
    static constexpr char const * TAG = "FormViewer";

    static constexpr uint8_t MAX_FORM_ENTRY   =  10;
    static constexpr uint8_t MAX_CHOICE_ENTRY =  30;
    static constexpr uint8_t FONT_SIZE        =   9;
    static constexpr uint8_t TOP_YPOS         = 100;
    static constexpr uint8_t BOTTOM_YPOS      =  50;

    struct ChoiceLoc {
      Pos pos;
      Dim dim;
      ChoiceLoc() {}
    } choice_loc[MAX_CHOICE_ENTRY];

    struct EntryLoc {
      Pos         pos;
      Dim         dim;
      int16_t     choices_height;
      ChoiceLoc * choices_loc;
      int8_t      first_choice_loc_idx;
      int8_t      choice_idx;
      EntryLoc() {}
    } entries_info[MAX_FORM_ENTRY];

    uint8_t entry_count;
    int16_t all_choices_width;
    // int16_t last_choices_width;
    int8_t  current_entry_idx;
    int8_t  line_height;
    bool    entry_selection;
    bool    highlight_selection;

    #if (INKPLATE_6PLUS || TOUCH_TRIAL)
      int8_t find_entry_idx(uint16_t x, uint16_t y);
      int8_t find_choice_idx(int8_t entry_idx, uint16_t x, uint16_t y);
    #endif

  public:
    enum class FormEntryType { HORIZONTAL, VERTICAL, DONE };

    struct FormEntry {
      const char   * caption;
      int8_t       * value;
      int8_t         choice_count;
      const Choice * choices;
      FormEntryType  entry_type;
    };

    typedef FormEntry * FormEntries;

    FormEntries entries;

    void show(FormEntries form_entries, int8_t size, const std::string & bottom_msg);
    bool event(EventMgr::Event event);
};

#if __FORM_VIEWER__
  FormViewer form_viewer;
#else
  extern FormViewer form_viewer;
#endif
