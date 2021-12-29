// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FORM_VIEWER__ 1
#include "viewers/form_viewer.hpp"

#include "models/fonts.hpp"
#include "viewers/page.hpp"
#include "screen.hpp"

MemoryPool<FormChoice::Item> FormChoice::item_pool;
uint8_t FormChoice::font_choices_count = 0;

Choice FormChoice::font_choices[8] = {
  { nullptr, 0 },
  { nullptr, 1 },
  { nullptr, 2 },
  { nullptr, 3 },
  { nullptr, 4 },
  { nullptr, 5 },
  { nullptr, 6 },
  { nullptr, 7 }
};

#if 0
void
FormViewer::show(FormViewer::FormEntries form_entries, int8_t size, const std::string & bottom_msg)
{
  // Compute location of items on screen

  entries = form_entries;

  Font *        font                      =  fonts.get(5);
  Font::Glyph * glyph                     =  font->get_glyph('M', FORM_FONT_SIZE);
  uint8_t       base_line_offset          = -glyph->yoff;
  uint8_t       next_available_choice_loc =  0;

  line_height = font->get_line_height(FORM_FONT_SIZE);
  entry_count = size;

  // Compute width / height of individual choices and entry captions

  for (int i = 0; i < entry_count; i++) {
    #if INKPLATE_6PLUS || TOUCH_TRIAL
      if (i < (entry_count - 1)) { // Don't do last entry (OK / CANCEL) or (DONE)
        font->get_size(entries[i].caption, &entries_info[i].dim, FORM_FONT_SIZE);
      }
      else {
        entries_info[i].dim.width = entries_info[i].dim.height = 0;
      }
    #else
      font->get_size(entries[i].caption, &entries_info[i].dim, FORM_FONT_SIZE);
    #endif
    entries_info[i].u.choice.first_choice_loc_idx = next_available_choice_loc;

    for (int j = 0, k = next_available_choice_loc; j < entries[i].choice_count; j++, k++) {
      font->get_size(
        entries[i].choices[j].caption, 
        &choice_loc[k].dim, 
        FORM_FONT_SIZE);
      if (entries[i].choices[j].value == * (uint8_t *) entries[i].value) {
        entries_info[i].u.choice.choice_idx = k;
      }
    }
    next_available_choice_loc += entries[i].choice_count;
  }

  int16_t       current_ypos = TOP_YPOS + 10;
  const int16_t right_xpos   = Screen::WIDTH - 60;

  // Compute combined width / height

  #if INKPLATE_6PLUS || TOUCH_TRIAL
    for (int i = 0; i < entry_count - 1; i++) {
  #else
    for (int i = 0; i < entry_count; i++) {
  #endif

    if (entries[i].entry_type == FormEntryType::UINT16) {

    }
    else {
      int16_t choices_width  = 0;
      int16_t choices_height = 0;
      int16_t separator      = 0;
      int16_t last_height    = 0;

      if (entries[i].entry_type == FormEntryType::HORIZONTAL) {     
        for (int j = 0, k = entries_info[i].u.choice.first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
          if (choices_height < choice_loc[k].dim.height) choices_height = choice_loc[k].dim.height;
          choices_width += choice_loc[k].dim.width + separator;
          separator = 20;
        }
      }
      else if (entries[i].entry_type == FormEntryType::VERTICAL) { // VERTICAL
        for (int j = 0, k = entries_info[i].u.choice.first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
          if (choices_width < choice_loc[k].dim.width) choices_width = choice_loc[k].dim.width;
          choices_height += line_height;
          last_height     = choice_loc[k].dim.height;
        }
        choices_height += last_height - line_height;
      }
      if (all_choices_width < choices_width) all_choices_width = choices_width;
      entries_info[i].u.choice.choices_height = choices_height;
    }
  }

  // Compute xpos, ypos

  #if INKPLATE_6PLUS || TOUCH_TRIAL
    for (int i = 0; i < entry_count - 1; i++) {
  #else
    for (int i = 0; i < entry_count; i++) {
  #endif
    if (entries[i].entry_type == FormEntryType::HORIZONTAL) {     
      int16_t left_pos = right_xpos - all_choices_width - 10;
      for (int j = 0, k = entries_info[i].u.choice.first_choice_loc_idx; 
           j < entries[i].choice_count; 
           j++, k++) {
        choice_loc[k].pos.x = left_pos;
        choice_loc[k].pos.y = current_ypos + 10;
        left_pos += choice_loc[k].dim.width + 20;
      }
    }
    else { // VERTICAL
      int16_t top_ypos = current_ypos + 10;
      for (int j = 0, k = entries_info[i].u.choice.first_choice_loc_idx; 
           j < entries[i].choice_count; 
           j++, k++) {
        choice_loc[k].pos.x = right_xpos - all_choices_width - 10;
        choice_loc[k].pos.y = top_ypos;
        top_ypos += line_height;
      }
    }
    entries_info[i].pos.x = right_xpos - all_choices_width - 35 - entries_info[i].dim.width;
    entries_info[i].pos.y = current_ypos + 10;
    current_ypos         += entries_info[i].u.choice.choices_height + 20;
  }

  current_ypos += 20;

  #if INKPLATE_6PLUS || TOUCH_TRIAL
    int k = entries_info[entry_count - 1].u.choice.first_choice_loc_idx;
    // int k = entries_info[entry_count].first_choice_loc_idx;
    choice_loc[k].pos.y     = choice_loc[k + 1].pos.y = current_ypos;
    choice_loc[k].pos.x     = (Screen::WIDTH >> 1) - choice_loc[k].dim.width - 20;
    // choice_loc[k + 1].pos.x = (Screen::WIDTH >> 1) + 20;
    entries_info[entry_count - 1].u.choice.choices_height = choice_loc[k].dim.height;
    // entries_info[entry_count].choices_height = choice_loc[k].dim.height;
    //last_choices_width = choice_loc[k].dim.width + choice_loc[k + 1].dim.width + 40;
  #endif

  // Display the form

  Page::Format fmt = {
    .line_height_factor =   1.0,
    .font_index         =     5,
    .font_size          = FORM_FONT_SIZE,
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

  for (int i = 0; i < entry_count; i++) {
    if (entries[i].caption != nullptr) { // The last one has no caption
      page.put_str_at(
        entries[i].caption, 
        Pos(entries_info[i].pos.x, entries_info[i].pos.y + base_line_offset), 
        fmt);
    }

    for (int j = 0, k = entries_info[i].u.choice.first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
      page.put_str_at(
        entries[i].choices[j].caption,
        Pos(choice_loc[k].pos.x, choice_loc[k].pos.y + base_line_offset),
        fmt);
    }
  }

  fmt.screen_top = current_ypos + 40;
  page.set_limits(fmt);
  page.new_paragraph(fmt);
  if (!bottom_msg.empty()) {
    page.add_text(
      bottom_msg,
      fmt
    );
  }
  page.end_paragraph(fmt);

  // Highlight current values

  for (int i = 0; i < entry_count; i++) {
    int8_t k = entries_info[i].u.choice.choice_idx;

    if (entries[i].entry_type == FormEntryType::DONE) {
      page.put_rounded(
        Dim(choice_loc[k].dim.width  + 16,
            choice_loc[k].dim.height + 16),
        Pos(choice_loc[k].pos.x - 8, choice_loc[k].pos.y - 8));
      page.put_rounded(
        Dim(choice_loc[k].dim.width  + 18,
            choice_loc[k].dim.height + 18),
        Pos(choice_loc[k].pos.x - 9, choice_loc[k].pos.y - 9));
      page.put_rounded(
        Dim(choice_loc[k].dim.width  + 20,
            choice_loc[k].dim.height + 20),
        Pos(choice_loc[k].pos.x - 10, choice_loc[k].pos.y - 10));
    }
    else {
      page.put_highlight(
        Dim(entries[i].entry_type == FormEntryType::HORIZONTAL ? choice_loc[k].dim.width + 10 : all_choices_width + 10,
            choice_loc[k].dim.height + 10),
        Pos(choice_loc[k].pos.x - 5, choice_loc[k].pos.y - 5));
    }
  }

  // Set current entry, highlighting the set of choices for that entry

  current_entry_idx   = 0;
  highlight_selection = false;

  #if (INKPLATE_6PLUS || TOUCH_TRIAL)
    entry_selection = false;
  #else
    entry_selection = true;
    page.put_highlight(
      Dim(all_choices_width + 20,
          entries_info[0].u.choice.choices_height + 20),
      Pos(choice_loc[entries_info[0].u.choice.first_choice_loc_idx].pos.x - 10,
          choice_loc[entries_info[0].u.choice.first_choice_loc_idx].pos.y - 10));
  #endif
  
  page.paint(false);
}

#endif

#if (INKPLATE_6PLUS || TOUCH_TRIAL)
  Fields::iterator
  FormViewer::find_field(uint16_t x, uint16_t y)
  {
    for (Fields::iterator it = fields.begin(); it < fields.end(); it++) {
      if ((*it)->is_pointed(x, y)) return it;
    }

    return fields.end();
  }

  int8_t
  FormViewer::find_choice_idx(int8_t entry_idx, uint16_t x, uint16_t y)
  {
    for (int8_t idx = entries_info[entry_idx].u.choice.first_choice_loc_idx, 
         k = 0; 
         k < entries[entry_idx].choice_count; idx++, k++) {
      if ((x >=  choice_loc[idx].pos.x - 10) &&
          (x <= (choice_loc[idx].pos.x + choice_loc[idx].dim.width + 10)) &&
          (y >=  choice_loc[idx].pos.y - 10) &&
          (y <= (choice_loc[idx].pos.y + choice_loc[idx].dim.height + 10))) {
        LOG_D("Found choice loc: %d", idx);
        return idx;
      }
    }

    return -1;
  }
#endif

bool
FormViewer::event(const EventMgr::Event & event)
{
  bool completed = false;
  
  #if (INKPLATE_6PLUS || TOUCH_TRIAL)
    switch (event.kind) {
      case EventMgr::EventKind::TAP:
        current_field = find_field(event.x, event.y);

        if (current_field != fields.end()) {
          completed = (*current_field)->select_item(event.x, event.y);
        }
        break;

      default:
        break;
    }
  #else
    Fields::iterator old_field = current_field;
    if (field_focus) {
      switch (event.kind) {
        case EventMgr::EventKind::DBL_PREV:
        case EventMgr::EventKind::PREV:
          if (current_field == fields.begin()) current_field = fields.end();
          current_field--;
          break;
        case EventMgr::EventKind::DBL_NEXT:
        case EventMgr::EventKind::NEXT:
          current_field++;
          if (current_field == fields.end()) current_field = fields.begin();
          break;
        case EventMgr::EventKind::SELECT:
          field_focus = false;
          field_select = true;
          break;
        case EventMgr::EventKind::NONE:
          return false;
        case EventMgr::EventKind::DBL_SELECT:
          completed = true;
          break;
      }
    }
    else {
      (*current_field)->event(event);

      switch (event.kind) {
        case EventMgr::EventKind::SELECT:
          field_focus = true;
          old_field = current_field;
          current_field++;
          if (current_field == fields.end()) current_field = fields.begin();
          break;
        case EventMgr::EventKind::NONE:
          return false;
        case EventMgr::EventKind::DBL_SELECT:
          completed = true;
          break;
        default:
          break;
      }
    }
  #endif
  
  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   5,
    .font_size          = FORM_FONT_SIZE,
    .indent             =   0,
    .margin_left        =   0,
    .margin_right       =   0,
    .margin_top         =   0,
    .margin_bottom      =   0,
    .screen_left        =  20,
    .screen_right       =  20,
    .screen_top         =  TOP_YPOS,
    .screen_bottom      =  BOTTOM_YPOS,
    .width              =   0,
    .height             =   0,
    .vertical_align     =   0,
    .trim               = true,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = CSS::Align::LEFT,
    .text_transform     = CSS::TextTransform::NONE,
    .display            = CSS::Display::INLINE
  };

  page.start(fmt);

  if (!completed) {

    if (field_focus) {
//      if (current_entry_idx != old_entry_idx) {
        #if !(INKPLATE_6PLUS || TOUCH_TRIAL)
          (*old_field)->select(false);
          (*current_field)->focus(true);
        #endif
//      }
    }
    else {
      #if !(INKPLATE_6PLUS || TOUCH_TRIAL)
        if (field_select) {
          field_select = false;
          (*current_field)->select(true);
        }
      #endif

      for (auto * field : fields) {
        field->update_highlight();
      }
    }

    page.paint(false);
  }
  else {
    for (auto * field : fields) field->save_value();

    page.clear_region(
      Dim(Screen::WIDTH - 40, Screen::HEIGHT - fmt.screen_bottom - fmt.screen_top),
      Pos(20, TOP_YPOS));

    page.paint(false);
  }
  return completed;
}