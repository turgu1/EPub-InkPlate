// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"
#include "models/fonts.hpp"
#include "viewers/page.hpp"
#include "memory_pool.hpp"

#include <list>

enum class FormEntryType { HORIZONTAL, VERTICAL, UINT16, DONE };

struct Choice {
  const char   * caption;
  int8_t         value;
};

struct FormEntry {
  const char   * caption;
  void         * value;
  int8_t         choice_count;
  Choice       * choices;
  FormEntryType  entry_type;
};

class FormField
{
  public:
    static constexpr uint8_t FONT_SIZE = 9;
    FormField(FormEntry & form_entry, Font & font) :
      form_entry(form_entry), font(font) { 

      compute_caption_dim();
      compute_field_dim();
    };
   ~FormField() { };

    inline const Dim & get_field_dim() { return field_dim; }

    virtual void             paint(Page::Format & fmt) = 0;
    virtual void compute_field_pos(Pos from_pos) = 0;

    void compute_caption_pos(Pos from_pos) {
      caption_pos = { from_pos.x - caption_dim.width, from_pos.y };
    }

    void focus(bool show_it) {
      if (show_it) {
        page.put_highlight(Dim(field_dim.width + 20, field_dim.height + 20),
                           Pos(field_pos.x - 10, field_pos.y - 10));
      }
      else {
        page.clear_highlight(Dim(field_dim.width + 20, field_dim.height + 20),
                             Pos(field_pos.x -10, field_pos.y - 10));
      }
    }

    void select(bool show_it) {
      if (show_it) {
        page.put_highlight(Dim(field_dim.width + 20, field_dim.height + 20),
                           Pos(field_pos.x - 10, field_pos.y - 10));
        page.put_highlight(Dim(field_dim.width + 22, field_dim.height + 22),
                           Pos(field_pos.x - 11, field_pos.y - 11));
        page.put_highlight(Dim(field_dim.width + 24, field_dim.height + 24),
                           Pos(field_pos.x - 12, field_pos.y - 12));
      }
      else {
        page.clear_highlight(Dim(field_dim.width + 20, field_dim.height + 20),
                             Pos(field_pos.x -10, field_pos.y - 10));
        page.clear_highlight(Dim(field_dim.width + 22, field_dim.height + 22),
                             Pos(field_pos.x -11, field_pos.y - 11));
        page.clear_highlight(Dim(field_dim.width + 24, field_dim.height + 24),
                             Pos(field_pos.x -12, field_pos.y - 12));
      }
    }

    #if (INKPLATE_6PLUS || TOUCH_TRIAL)
      inline bool is_pointed(uint16_t x. uint16_t y) {
        return (x >= (field_pos.x - 10)) && 
               (x <= (field_pos.x + field_dim.width + 10)) &&
               (y >= (field_pos.y - 10)) &&
               (y <= (field_pos.y + field_dim.height + 10));
      } 
    #endif

  protected:
    
    Font      & font;
    FormEntry & form_entry;
    Dim         field_dim, caption_dim;
    Pos         field_pos, caption_pos;

    virtual void compute_field_dim() = 0;

    void compute_caption_dim() {
      font.get_size(form_entry.caption, &caption_dim, FONT_SIZE);
    }
};

class FormChoice : public FormField
{
  public:

    static constexpr Choice done_choices[1] = {
      { "DONE",       1 }
    };

    static constexpr Choice dir_view_choices[2] = {
      { "LINEAR",     0 },
      { "MATRIX",     1 }
    };

    static constexpr Choice ok_cancel_choices[2] = {
      { "OK",         1 },
      { "CANCEL",     0 }
    };

    static constexpr Choice yes_no_choices[2] = {
      { "YES",        1 },
      { "NO",         0 }
    };

    static constexpr Choice resolution_choices[2] = {
      { "1Bit",       0 },
      { "3Bits",      1 }
    };

    static constexpr Choice timeout_choices[3] = {
      { "5",          5 },
      { "15",        15 },
      { "30",        30 }
    };

    static constexpr Choice battery_visual_choices[4] = {
      { "NONE",       0 },
      { "PERCENT",    1 },
      { "VOLTAGE",    2 },
      { "ICON",       3 }
    };

    static constexpr Choice font_size_choices[4] = {
      { "8",          8 },
      { "10",        10 },
      { "12",        12 },
      { "15",        15 }
    };

    #if (INKPLATE_6PLUS || TOUCH_TRIAL)
      static constexpr orientation_choices[4] = {
        { "LEFT",     3 },
        { "RIGHT",    2 },
        { "TOP",      1 },
        { "BOTTOM",   0 }
      };
    #else
      static constexpr Choice orientation_choices[3] = {
        { "LEFT",     0 },
        { "RIGHT",    1 },
        { "BOTTOM",   2 }
      };
    #endif

    static Choice font_choices[8];
    uint8_t font_choices_count;

    void adjust_font_choices(char ** font_names, uint8_t size) {
      for (uint8_t i = 0; i < size; i++) font_choices[i].caption = font_names[i];
      font_choices_count = size; 
    }

    inline uint8_t get_font_choices_count() { return font_choices_count; }
    void compute_field_pos(Pos from_pos) = 0;

    void paint(Page::Format & fmt) {
      page.put_str_at(form_entry.caption, caption_pos, fmt);
      uint8_t i = 0;
      for (auto * item : items) {
        page.put_str_at(form_entry.choices[i].caption, item->pos, fmt);
        i++;
      }
    }

  protected:
    struct Item {
      Pos pos;
      Dim dim;
    };

    static MemoryPool<Item> item_pool; // Shared by all FormChoice

    typedef std::list<Item *> Items;
    
    Items items;
    Items::iterator current_item;

    void compute_field_dim() {
      field_dim = { 0, 0 };
      for (int i = 0; i < form_entry.choice_count; i++) {
        Item * item = item_pool.newElement();
        items.push_back(item);
        font.get_size(form_entry.choices[i].caption, &item->dim, FONT_SIZE);
      }

      current_item = items.begin();
      
      int i = 0;
      for (Items::iterator it = items.begin(); it != items.end(); it++) {
        if (form_entry.choices[i].value == * (uint8_t *) form_entry.value) {
          current_item = it;
          break;
        }
        i++;        
      }
    }

  public:
    using FormField::FormField;

   ~FormChoice() {
      for (auto * item : items) {
        item_pool.deleteElement(item);
      }
      items.clear();
    }
};

class VFormChoice : public FormChoice
{
  public:
    using FormChoice::FormChoice;

    void compute_field_pos(Pos from_pos) { field_pos = from_pos; }
    
  protected:
    void compute_field_dim() {
      FormChoice::compute_field_dim();
      uint8_t line_height = font.get_line_height(FONT_SIZE);
      uint8_t last_height = 0;
      for (auto * item : items) {
        if (field_dim.width < item->dim.width) field_dim.width = item->dim.width;
        field_dim.height += line_height;
        last_height = item->dim.height;
      }
      field_dim.height += last_height - line_height;
    }
};

class HFormChoice : public FormChoice
{
  public:
    using FormChoice::FormChoice;

    void compute_field_pos(Pos from_pos) { field_pos = from_pos; }
    
  protected:
    static constexpr uint8_t HORIZONTAL_SEPARATOR = 20;

    void compute_field_dim() {
      FormChoice::compute_field_dim();
      uint16_t separator = 0;
      for (auto * item : items) {
        if (field_dim.height < item->dim.height) field_dim.height = item->dim.height;
        field_dim.width += item->dim.width + separator;
        separator = HORIZONTAL_SEPARATOR;
      }
    }
};

class FormUInt16 : public FormField
{
  public:
    using FormField::FormField;

    void compute_field_pos(Pos from_pos) { field_pos = from_pos; }
    void paint(Page::Format & fmt) {
      char val[8];
      int_to_str(* (uint16_t *) form_entry.value, val, 8);
      page.put_str_at(form_entry.caption, caption_pos, fmt);
      page.put_str_at(val, field_pos, fmt);
    }

  protected:
    void compute_field_dim() {
      font.get_size("XXXXX", &field_dim, FONT_SIZE);
    }
};

class FieldFactory
{
  public:
    static FormField * create(FormEntry & entry, Font & font) {
      switch (entry.entry_type) {
        case FormEntryType::HORIZONTAL:
          return new HFormChoice(entry, font);
        case FormEntryType::VERTICAL:
          return new VFormChoice(entry, font);
        case FormEntryType::UINT16:
          return new FormUInt16(entry, font);
        case FormEntryType::DONE:
          return nullptr;
      }
    }
};

class FormViewer
{
  private:
    static constexpr char const * TAG = "FormViewer";

    static constexpr uint8_t TOP_YPOS         = 100;
    static constexpr uint8_t BOTTOM_YPOS      =  50;

    uint8_t entry_count;
    int16_t all_fields_width;
    // int16_t last_choices_width;
    int8_t  line_height;
    bool    field_focus;
    bool    field_select;

    typedef std::list<FormField *> Fields;
    
    Fields fields;
    Fields::iterator current_field;

    #if (INKPLATE_6PLUS || TOUCH_TRIAL)
      int8_t find_entry_idx(uint16_t x, uint16_t y);
      int8_t find_choice_idx(int8_t entry_idx, uint16_t x, uint16_t y);
    #endif

  public:

    typedef FormEntry * FormEntries;


    void show(FormEntries form_entries, int8_t size, const std::string & bottom_msg) {

      Font *        font                      =  fonts.get(5);
      Font::Glyph * glyph                     =  font->get_glyph('M', FormField::FONT_SIZE);
      uint8_t       base_line_offset          = -glyph->yoff;

      for (auto * field : fields) delete field;
      fields.clear();

      for (int i = 0; i < size; i++) {
        FormField * field = FieldFactory::create(form_entries[i], *font); 
        if (field != nullptr) fields.push_back(field);
      }

      all_fields_width = 0;
      for (auto * field : fields) {
        int16_t width = field->get_field_dim().width;
        if (width > all_fields_width) all_fields_width = width;
      }

      int16_t       current_ypos  = TOP_YPOS + 10;
      const int16_t right_xpos    = Screen::WIDTH - 60;
      int16_t       caption_right = right_xpos - all_fields_width - 35;
      int16_t       field_left    = right_xpos - all_fields_width - 10;

      for (auto * field : fields) {
        field->compute_caption_pos(Pos(caption_right, current_ypos + 10));
        field->compute_field_pos(Pos(field_left, current_ypos + 10));
        current_ypos += field->get_field_dim().height + 20;
      }

      // Display the form

      Page::Format fmt = {
        .line_height_factor =   1.0,
        .font_index         =     5,
        .font_size          = FormField::FONT_SIZE,
        .indent             =     0,
        .margin_left        =     5,
        .margin_right       =     5,
        .margin_top         =     0,
        .margin_bottom      =     0,
        .screen_left        =    20,
        .screen_right       =    20,
        .screen_top         = TOP_YPOS,
        .screen_bottom      = BOTTOM_YPOS,
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

      // The large rectangle into which the form will be drawn

      page.clear_region(
        Dim(Screen::WIDTH - 40, Screen::HEIGHT - fmt.screen_bottom - fmt.screen_top),
        Pos(20, TOP_YPOS));

      page.put_highlight(
        Dim(Screen::WIDTH - 44, Screen::HEIGHT - fmt.screen_bottom - fmt.screen_top - 4),
        Pos(22, TOP_YPOS + 2));

      // Show all captions (but the last one (OK / CANCEL) or (DONE)) and choices 

      for (auto * field : fields) {
        field->paint(fmt);
      }

      current_field = fields.begin();

      field_select = false;

      #if (INKPLATE_6PLUS || TOUCH_TRIAL)
        field_focus = false;
      #else
        field_focus = true;
        (*current_field)->focus(true);
      #endif

      page.paint(false);
    }

    bool event(const EventMgr::Event & event);
};

#if __FORM_VIEWER__
  FormViewer form_viewer;
#else
  extern FormViewer form_viewer;
#endif
