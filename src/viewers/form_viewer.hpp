// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"
#include "memory_pool.hpp"

#include "controllers/event_mgr.hpp"
#include "models/fonts.hpp"
#include "viewers/keypad_viewer.hpp"
#include "viewers/page.hpp"
#include "viewers/screen_bottom.hpp"

#include <list>

enum class FormEntryType {
  HORIZONTAL, VERTICAL, UINT16,
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    DONE
  #endif
};

constexpr uint8_t FORM_FONT_SIZE = 9;

struct FormChoice {
  const char *caption;
  int8_t value;
};

struct FormEntry {
  const char *caption;
  union {
    struct {
      int8_t *value;
      int8_t choice_count;
      const FormChoice *choices;
    } ch;
    struct {
      uint16_t *value;
      uint16_t min;
      uint16_t max;
    } val;
  } u;
  FormEntryType entry_type;
};

using FormFieldPtr = himem_unique_ptr<class FormField>;
class FormField {
private:
  static constexpr char const *TAG = "FormField";

protected:
  FormEntry &form_entry;
  Font &font;
  Page &page;
  bool event_control;
  Dim field_dim, caption_dim;
  Pos field_pos, caption_pos;

public:
  FormField(FormEntry &form_entry, Font &font, Page &page)
      : form_entry(form_entry), font(font), page(page), event_control(false) {};
  virtual ~FormField() { LOG_I("FormField destructor called"); }

  virtual const Dim get_field_dim() { return field_dim; }
  inline const Dim &get_caption_dim() { return caption_dim; }

  inline const Pos &get_field_pos() { return field_pos; }
  inline const Pos &get_caption_pos() { return caption_pos; }

  inline FormEntryType get_type() { return form_entry.entry_type; }

  void compute_caption_dim() {
    if (form_entry.caption != nullptr) {
      font.get_size(form_entry.caption, &caption_dim, FORM_FONT_SIZE);
    } else {
      caption_dim = Dim(0, 0);
    }
  }

  virtual bool form_refresh_required() { return false; }
  virtual void paint(Page::Format &fmt)        = 0;
  virtual void compute_field_dim()             = 0;
  virtual void compute_field_pos(Pos from_pos) = 0;
  virtual void update_highlight()              = 0;
  virtual void save_value()                    = 0;

  virtual bool event(const EventMgr::Event &event) { return false; }

  inline bool in_event_control() { return event_control; }

  void compute_caption_pos(Pos from_pos) {
    caption_pos = Pos(from_pos.x - caption_dim.width, from_pos.y);
  }

  void show_highlighted(bool show_it) {
    if (show_it) {
      page.put_highlight(Dim(field_dim.width + 20, field_dim.height + 20),
                         Pos(field_pos.x - 10, field_pos.y - 10));
    } else {
      page.clear_highlight(Dim(field_dim.width + 20, field_dim.height + 20),
                           Pos(field_pos.x - 10, field_pos.y - 10));
    }
  }

  void show_selected(bool show_it) {
    if (show_it) {
      page.put_highlight(Dim(field_dim.width + 20, field_dim.height + 20),
                         Pos(field_pos.x - 10, field_pos.y - 10));
      page.put_highlight(Dim(field_dim.width + 22, field_dim.height + 22),
                         Pos(field_pos.x - 11, field_pos.y - 11));
      page.put_highlight(Dim(field_dim.width + 24, field_dim.height + 24),
                         Pos(field_pos.x - 12, field_pos.y - 12));
    } else {
      page.clear_highlight(Dim(field_dim.width + 20, field_dim.height + 20),
                           Pos(field_pos.x - 10, field_pos.y - 10));
      page.clear_highlight(Dim(field_dim.width + 22, field_dim.height + 22),
                           Pos(field_pos.x - 11, field_pos.y - 11));
      page.clear_highlight(Dim(field_dim.width + 24, field_dim.height + 24),
                           Pos(field_pos.x - 12, field_pos.y - 12));
    }
  }

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    inline bool is_pointed(uint16_t x, uint16_t y) {
      return (x >= (field_pos.x - 10)) && (y >= (field_pos.y - 10)) &&
             (x <= (field_pos.x + field_dim.width + 10)) &&
             (y <= (field_pos.y + field_dim.height + 10));
    }

  #endif
};

class FormChoiceField : public FormField {
public:
  static constexpr FormChoice dir_view_choices[2] = {{"LINEAR", 0}, {"MATRIX", 1}};

  static constexpr FormChoice cover_size_choices[3] = {{"SMALL", 0}, {"MEDIUM", 1}, {"LARGE", 2}};

  static constexpr FormChoice ok_cancel_choices[2] = {{"OK", 1}, {"CANCEL", 0}};

  static constexpr FormChoice yes_no_choices[2] = {{"YES", 1}, {"NO", 0}};

  static constexpr FormChoice resolution_choices[2] = {{"1Bit", 0}, {"3Bits", 1}};

  static constexpr FormChoice timeout_choices[3] = {{"5", 5}, {"15", 15}, {"30", 30}};

  static constexpr FormChoice battery_visual_choices[4] = {
      {"NONE", 0}, {"PERCENT", 1}, {"VOLTAGE", 2}, {"ICON", 3}};

  static constexpr FormChoice font_size_choices[4] = {{"8", 8}, {"10", 10}, {"12", 12}, {"15", 15}};

  #if DATE_TIME_RTC
    static constexpr FormChoice right_corner_choices[3] = {
        {"NONE", 0}, {"DATE TIME", 1}, {"HEAP", 2}};
  #endif

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    static constexpr FormChoice orientation_choices[4] = {
        {"LEFT", 3}, {"RIGHT", 2}, {"TOP", 1}, {"BOTTOM", 0}};
  #else
    static constexpr FormChoice orientation_choices[3] = {{"LEFT", 0}, {"RIGHT", 1}, {"BOTTOM", 2}};
  #endif

  static FormChoice font_choices[8];
  static uint8_t font_choices_count;

  void compute_field_dim() {
    field_dim = {0, 0};
    for (int8_t i = 0; i < form_entry.u.ch.choice_count; i++) {
      Item *item = item_pool.newElement();
      items.push_back(item);
      font.get_size(form_entry.u.ch.choices[i].caption, &item->dim, FORM_FONT_SIZE);
      item->idx = i;
    }

    int i        = 0;
    current_item = items.end();
    for (Items::iterator it = items.begin(); it != items.end(); it++) {
      if (form_entry.u.ch.choices[i].value == *form_entry.u.ch.value) {
        current_item = it;
        break;
      }
      i++;
    }
    if (current_item == items.end()) current_item = items.begin();

    old_item = items.end();
  }

  static void adjust_font_choices(char **font_names, uint8_t size) {
    for (uint8_t i = 0; i < size; i++) font_choices[i].caption = font_names[i];
    font_choices_count = size;
  }

  void compute_field_pos(Pos from_pos) = 0;

  void paint(Page::Format &fmt) {

    Glyph *glyph   = font.get_glyph('M', FORM_FONT_SIZE);
    uint8_t offset = -glyph->yoff;

    page.put_str_at(form_entry.caption, Pos(caption_pos.x, caption_pos.y + offset), fmt);
    for (auto *item : items) {
      page.put_str_at(form_entry.u.ch.choices[item->idx].caption,
                      Pos(item->pos.x, item->pos.y + offset), fmt);
    }
  }

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    bool event(const EventMgr::Event &event) {
      old_item = current_item;
      Items::iterator it;
      for (it = items.begin(); it != items.end(); it++) {
        if ((event.x >= (*it)->pos.x - 5) && (event.y >= (*it)->pos.y - 5) &&
            (event.x <= ((*it)->pos.x + (*it)->dim.width + 5)) &&
            (event.y <= ((*it)->pos.y + (*it)->dim.height + 5))) {
          break;
        }
      }
      if (it != items.end()) current_item = it;
      return false;
    }
  #else
    bool event(const EventMgr::Event &event) {
      old_item = current_item;
      switch (event.kind) {
      case EventMgr::EventKind::DBL_PREV:
      case EventMgr::EventKind::PREV:
        if (current_item == items.begin()) current_item = items.end();
        current_item--;
        break;
      case EventMgr::EventKind::DBL_NEXT:
      case EventMgr::EventKind::NEXT:
        current_item++;
        if (current_item == items.end()) current_item = items.begin();
        break;
      default:
        break;
      }
      return false;
    }
  #endif

  void update_highlight() {
    if (old_item != current_item) {
      if (old_item != items.end()) {
        page.clear_highlight(Dim((*old_item)->dim.width + 10, (*old_item)->dim.height + 10),
                             Pos((*old_item)->pos.x - 5, (*old_item)->pos.y - 5));
      }
      page.put_highlight(Dim((*current_item)->dim.width + 10, (*current_item)->dim.height + 10),
                         Pos((*current_item)->pos.x - 5, (*current_item)->pos.y - 5));
    }
    old_item = current_item;
  }

  void save_value() {
    *form_entry.u.ch.value = form_entry.u.ch.choices[(*current_item)->idx].value;
  }

protected:
  struct Item {
    Pos pos;
    Dim dim;
    uint8_t idx;
  };

  static MemoryPool<Item> item_pool; // Shared by all FormChoiceField

  typedef std::list<Item *> Items;

  Items items;
  Items::iterator current_item, old_item;

public:
  using FormField::FormField;

  ~FormChoiceField() {
    for (auto *item : items) {
      item_pool.deleteElement(item);
    }
    items.clear();
  }
};

class VFormChoiceField : public FormChoiceField {
private:
  static constexpr char const *TAG = "VFormChoiceField";

public:
  using FormChoiceField::FormChoiceField;

  VFormChoiceField()  = default;
  ~VFormChoiceField() = default;

  static inline auto Make(FormEntry &form_entry, Font &font, Page &page) {
    return make_unique_himem<VFormChoiceField>(form_entry, font, page);
  }

  void compute_field_pos(Pos from_pos) {
    field_pos   = from_pos;
    Pos the_pos = from_pos;

    uint8_t line_height = font.get_line_height(FORM_FONT_SIZE);
    for (auto *item : items) {
      item->pos = the_pos;
      the_pos.y += line_height;
      LOG_D("Item position  [%d, %d]", item->pos.x, item->pos.y);
    }
  }

  void compute_field_dim() {
    FormChoiceField::compute_field_dim();
    uint8_t line_height = font.get_line_height(FORM_FONT_SIZE);
    uint8_t last_height = 0;
    for (auto *item : items) {
      if (field_dim.width < item->dim.width) field_dim.width = item->dim.width;
      field_dim.height += line_height;
      last_height = item->dim.height;
      LOG_D("Item dimension: [%d, %d]", item->dim.width, item->dim.height);
    }
    field_dim.height += last_height - line_height;
  }
};

using HFormChoiceFieldPtr = himem_unique_ptr<class HFormChoiceField>;

class HFormChoiceField : public FormChoiceField {
private:
  static constexpr char const *TAG = "HFormChoiceField";

public:
  using FormChoiceField::FormChoiceField;

  HFormChoiceField()  = default;
  ~HFormChoiceField() = default;

  static inline auto Make(FormEntry &form_entry, Font &font, Page &page) {
    return make_unique_himem<HFormChoiceField>(form_entry, font, page);
  }

  void compute_field_pos(Pos from_pos) {
    field_pos   = from_pos;
    Pos the_pos = from_pos;

    for (auto *item : items) {
      item->pos = the_pos;
      the_pos.x += item->dim.width + 20;
      LOG_D("Item position: [%d, %d]", item->pos.x, item->pos.y);
    }
  }

  static constexpr uint8_t HORIZONTAL_SEPARATOR = 20;

  void compute_field_dim() {
    FormChoiceField::compute_field_dim();
    uint16_t separator = 0;
    for (auto *item : items) {
      if (field_dim.height < item->dim.height) field_dim.height = item->dim.height;
      field_dim.width += item->dim.width + separator;
      separator = HORIZONTAL_SEPARATOR;
      LOG_D("Item dimension: [%d, %d]", item->dim.width, item->dim.height);
    }
  }
};

using FormUInt16Ptr = himem_unique_ptr<class FormUInt16>;

class FormUInt16 : public FormField {
private:
  KeypadViewerPtr keypad_viewer{KeypadViewer::Make()};

public:
  using FormField::FormField;

  FormUInt16()  = default;
  ~FormUInt16() = default;

  static inline auto Make(FormEntry &form_entry, Font &font, Page &page) {
    return make_unique_himem<FormUInt16>(form_entry, font, page);
  }

  bool form_refresh_required() { return true; }

  void compute_field_pos(Pos from_pos) { field_pos = from_pos; }

  void paint(Page::Format &fmt) {
    char val[8];
    Glyph *glyph   = font.get_glyph('M', FORM_FONT_SIZE);
    uint8_t offset = -glyph->yoff;

    uint16_t v = *form_entry.u.val.value;
    if (v < form_entry.u.val.min) {
      v = form_entry.u.val.min;
    } else if (v > form_entry.u.val.max) {
      v = form_entry.u.val.max;
    }
    *form_entry.u.val.value = v;

    int_to_str(v, val, 8);
    page.put_str_at(form_entry.caption, Pos(caption_pos.x, caption_pos.y + offset), fmt);
    page.put_str_at(val, Pos(field_pos.x, field_pos.y + offset), fmt);
  }

  bool event(const EventMgr::Event &event) {
    if (!event_control) {
      keypad_viewer->show(*form_entry.u.val.value, form_entry.caption);
      event_control = true;
    } else {
      if (!keypad_viewer->event(event)) {
        uint16_t v = keypad_viewer->get_value();
        if (v < form_entry.u.val.min) {
          v = form_entry.u.val.min;
        } else if (v > form_entry.u.val.max) {
          v = form_entry.u.val.max;
        }
        *form_entry.u.val.value = v;
        event_control           = false;

        return false; // release events control
      }
    }
    return true; // keep events control
  }

  void update_highlight() {}

  void save_value() {}

  void compute_field_dim() { font.get_size("XXXXX", &field_dim, FORM_FONT_SIZE); }
};

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  class FormDone : public FormField {

  public:
    using FormField::FormField;

    FormDone()  = default;
    ~FormDone() = default;

    static inline auto Make(FormEntry &form_entry, Font &font, Page &page) {
      return make_unique_himem<FormDone>(form_entry, font, page);
    }

    // bool event(const EventMgr::Event &event);

    const Dim get_field_dim() { return Dim(field_dim.width, field_dim.height + 10); }
    void save_value() {}
    void update_highlight() {
      page.put_rounded(Dim(field_dim.width + 16, field_dim.height + 16),
                       Pos(field_pos.x - 8, field_pos.y - 8));
      page.put_rounded(Dim(field_dim.width + 18, field_dim.height + 18),
                       Pos(field_pos.x - 9, field_pos.y - 9));
      page.put_rounded(Dim(field_dim.width + 20, field_dim.height + 20),
                       Pos(field_pos.x - 10, field_pos.y - 10));
    }

    void compute_field_dim() { font.get_size(form_entry.caption, &field_dim, FORM_FONT_SIZE); }

    void compute_field_pos(Pos from_pos) {
      field_pos.x = (Screen::get_width() / 2) - (field_dim.width / 2);
      field_pos.y = from_pos.y + 10;
    }

    void paint(Page::Format &fmt) {
      Glyph *glyph   = font.get_glyph('M', FORM_FONT_SIZE);
      uint8_t offset = -glyph->yoff;

      page.put_str_at(form_entry.caption, Pos(field_pos.x, field_pos.y + offset), fmt);
    }
  };
#endif

class FieldFactory {
public:
  static auto create(FormEntry &entry, Font &font, Page &page) -> FormFieldPtr {
    switch (entry.entry_type) {
    case FormEntryType::HORIZONTAL:
      return HFormChoiceField::Make(entry, font, page);
    case FormEntryType::VERTICAL:
      if ((Screen::get_width() > Screen::get_height()) && (entry.u.ch.choice_count <= 4)) {
        return HFormChoiceField::Make(entry, font, page);
      } else {
        return VFormChoiceField::Make(entry, font, page);
      }
    case FormEntryType::UINT16:
      return FormUInt16::Make(entry, font, page);
      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      case FormEntryType::DONE:
        return FormDone::Make(entry, font, page);
      #endif
    }
    return nullptr;
  }
};

using FormViewerPtr = himem_unique_ptr<class FormViewer>;
class FormViewer {
private:
  FormViewer() = default;

  PagePtr page{Page::Make()};

public:
  typedef FormEntry *FormEntries;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make() { return make_unique_himem<FormViewer>(); }

  ~FormViewer() {
    LOG_I("FormViewer destructor called");
    fields.clear();
  }

private:
  static constexpr char const *TAG = "FormViewer";

  static constexpr uint8_t TOP_YPOS    = 100;
  static constexpr uint8_t BOTTOM_YPOS = 50;

  FormEntries form_entries;
  uint8_t size;
  const char *title;
  const char *bottom_msg;

  int16_t all_fields_width, all_captions_width;
  // int16_t last_choices_width;
  int8_t line_height;
  bool highlighting_field;
  bool selecting_field;
  bool completed;

  typedef std::list<FormFieldPtr> Fields;

  Fields fields;
  Fields::iterator current_field;

  Pos bottom_msg_pos;

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    Fields::iterator find_field(uint16_t x, uint16_t y) {
      for (Fields::iterator it = fields.begin(); it != fields.end(); it++) {
        if ((*it)->is_pointed(x, y)) return it;
      }

      return fields.end();
    }

  #endif

public:
  void set_completed(bool value) { completed = value; }

  void show(const char *_title, FormEntries _form_entries, int8_t _size, const char *_bottom_msg,
            bool refresh = false);
  bool event(const EventMgr::Event &event);
};
