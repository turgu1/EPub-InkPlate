// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FORM_VIEWER__ 1
#include "viewers/form_viewer.hpp"

#include "models/fonts.hpp"
#include "models/ttf2.hpp"
#include "viewers/page.hpp"
#include "screen.hpp"

void
FormViewer::show(FormViewer::FormEntries form_entries, int8_t size)
{
  // Compute location of items on screen

  entries = form_entries;

  TTF * font = fonts.get(1, FONT_SIZE);
  TTF::BitmapGlyph * glyph = font->get_glyph('M');

  uint8_t line_offset = -glyph->yoff;
  uint8_t next_available_choice_loc = 0;

  entry_count = size;

  for (int i = 0; i < entry_count; i++) {
    if (i < (entry_count - 1)) { // Don't do last entry (OK / CANCEL)
      font->get_size(entries[i].caption, entries_info[i].width, entries_info[i].height);
    }
    else {
      entries_info[i].width = entries_info[i].height = 0;
    }
    entries_info[i].first_choice_loc_idx = next_available_choice_loc;

    for (int j = 0, k = next_available_choice_loc; j < entries[i].choice_count; j++, k++) {
      font->get_size(
        entries[i].choices[j].caption, 
        choice_loc[k].width,
        choice_loc[k].height);
      if (entries[i].choices[j].value == *entries[i].value) {
        entries_info[i].choice_idx = k;
      }
    }
    next_available_choice_loc += entries[i].choice_count;
  }

  // Compute localisation

  int16_t current_ypos = TOP_YPOS + 10;
  const int16_t right_xpos = Screen::WIDTH - 60;

  for (int i = 0; i < entry_count - 1; i++) {

    int16_t choices_width = 0, choices_height = 0;
    int16_t separator = 0;
    
    if (entries[i].entry_type == HORIZONTAL_CHOICES) {     
      for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
        if (choices_height < choice_loc[k].height) choices_height = choice_loc[k].height;
        choices_width += choice_loc[k].width + separator;
        separator = 20;
      }
    }
    else { // VERTICAL_CHOICES
      for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
        if (choices_width < choice_loc[k].width) choices_width = choice_loc[k].width;
        choices_height += choice_loc[k].height + separator;
        separator = 0;
      }
    }
    if (all_choices_width < choices_width) all_choices_width = choices_width;
    entries_info[i].choices_height = choices_height;
  }
  for (int i = 0; i < entry_count - 1; i++) {
    if (entries[i].entry_type == HORIZONTAL_CHOICES) {     
      int16_t left_pos = right_xpos - all_choices_width - 10;
      for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
        choice_loc[k].xpos = left_pos;
        choice_loc[k].ypos = current_ypos + 10;
        left_pos += choice_loc[k].width + 20;
      }
    }
    else { // VERTICAL_CHOICES
      int16_t top_ypos = current_ypos + 10;
      for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
        choice_loc[k].xpos = right_xpos - all_choices_width - 10;
        choice_loc[k].ypos = top_ypos;
        top_ypos += choice_loc[k].height;
      }
    }
    entries_info[i].xpos = right_xpos - all_choices_width - 35 - entries_info[i].width;
    entries_info[i].ypos = current_ypos + 10;
    current_ypos += entries_info[i].choices_height + 10;
  }

  current_ypos += 20;
  int k = entries_info[entry_count - 1].first_choice_loc_idx;
  choice_loc[k].ypos = choice_loc[k + 1].ypos = current_ypos;
  choice_loc[k].xpos = (Screen::WIDTH >> 1) - choice_loc[k].width - 20;
  choice_loc[k + 1].xpos = (Screen::WIDTH >> 1) + 20;
  entries_info[entry_count - 1].choices_height = choice_loc[k].height;
  last_choices_width = choice_loc[k].width + choice_loc[k + 1].width + 40;

  // Display the form

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   1,
    .font_size          = FONT_SIZE,
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
    .trim               = true,
    .font_style         = Fonts::NORMAL,
    .align              = CSS::LEFT_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM
  };

  page.start(fmt);

  page.clear_region(
    Screen::WIDTH - 40, 
    Screen::HEIGHT - fmt.screen_bottom - fmt.screen_top,
    20, TOP_YPOS);

  page.put_highlight(
    Screen::WIDTH - 44, 
    Screen::HEIGHT - fmt.screen_bottom - fmt.screen_top + 4,
    22, TOP_YPOS + 2);

  // Show all captions (but the last one (OK / CANCEL))

  for (int i = 0; i < entry_count; i++) {
    if (i < (entry_count - 1)) {
      page.put_str_at(
        entries[i].caption, 
        entries_info[i].xpos, 
        entries_info[i].ypos + line_offset, 
        fmt);
    }

    for (int j = 0, k = entries_info[i].first_choice_loc_idx; j < entries[i].choice_count; j++, k++) {
      page.put_str_at(
        entries[i].choices[j].caption,
        choice_loc[k].xpos,
        choice_loc[k].ypos + line_offset,
        fmt);
    }
  }

  // Highlight current values

  for (int i = 0; i < entry_count; i++) {
    int8_t k = entries_info[i].choice_idx;

    page.put_highlight(
      entries[i].entry_type == HORIZONTAL_CHOICES ? choice_loc[k].width + 10 : all_choices_width + 10,
      choice_loc[k].height - 5,
      choice_loc[k].xpos - 5,
      choice_loc[k].ypos - 5);
  }

  // Set current entry, highlighting the set of choices for that entry

  current_entry_idx = 0;
  entry_selection = true;

  page.put_highlight(
    all_choices_width + 20,
    entries_info[0].choices_height + 5,
    choice_loc[entries_info[0].first_choice_loc_idx].xpos - 10,
    choice_loc[entries_info[0].first_choice_loc_idx].ypos - 10);
  page.put_highlight(
    all_choices_width + 22,
    entries_info[0].choices_height + 7,
    choice_loc[entries_info[0].first_choice_loc_idx].xpos - 11,
    choice_loc[entries_info[0].first_choice_loc_idx].ypos - 11);

  page.paint(false);
}

bool
FormViewer::event(EventMgr::KeyEvent key)
{
  int8_t old_entry_idx = current_entry_idx;
  int8_t old_choice_idx = 0;
  int8_t choice_idx = 0;

  bool completed = false;

  if (entry_selection) {
    switch (key) {
      case EventMgr::KEY_DBL_PREV:
      case EventMgr::KEY_PREV:
        current_entry_idx--;
        if (current_entry_idx < 0) {
          current_entry_idx = entry_count - 1;
        }
        break;
      case EventMgr::KEY_DBL_NEXT:
      case EventMgr::KEY_NEXT:
        current_entry_idx++;
        if (current_entry_idx >= entry_count) {
          current_entry_idx = 0;
        }
        break;
      case EventMgr::KEY_SELECT:
        entry_selection = false;
        return false;
      case EventMgr::KEY_NONE:
        return false;
      case EventMgr::KEY_DBL_SELECT:
        completed = true;
        break;
    }
  }
  else {
    old_choice_idx = choice_idx = entries_info[current_entry_idx].choice_idx;

    switch (key) {
      case EventMgr::KEY_DBL_PREV:
      case EventMgr::KEY_PREV:
        choice_idx--;
        if (choice_idx < entries_info[current_entry_idx].first_choice_loc_idx) {
          choice_idx = entries_info[current_entry_idx].first_choice_loc_idx + entries[current_entry_idx].choice_count - 1;
        }
        break;
      case EventMgr::KEY_DBL_NEXT:
      case EventMgr::KEY_NEXT:
        choice_idx++;
        if (choice_idx >= entries_info[current_entry_idx].first_choice_loc_idx + entries[current_entry_idx].choice_count) {
          choice_idx = entries_info[current_entry_idx].first_choice_loc_idx;
        }
        break;
      case EventMgr::KEY_SELECT:
        entry_selection = true;
        return false;
      case EventMgr::KEY_NONE:
        return false;
      case EventMgr::KEY_DBL_SELECT:
        completed = true;
        break;
    }
  }

  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   1,
    .font_size          = FONT_SIZE,
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
    .trim               = true,
    .font_style         = Fonts::NORMAL,
    .align              = CSS::LEFT_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM
  };

  page.start(fmt);

  if (!completed) {

    if (entry_selection) {
      if (current_entry_idx != old_entry_idx) {
        page.clear_highlight(
          ((old_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 20,
          entries_info[old_entry_idx].choices_height + 5,
          choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].xpos - 10,
          choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].ypos - 10);
        page.clear_highlight(
          ((old_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 22,
          entries_info[old_entry_idx].choices_height + 7,
          choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].xpos - 11,
          choice_loc[entries_info[old_entry_idx].first_choice_loc_idx].ypos - 11);

        page.put_highlight(
          ((current_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 20,
          entries_info[current_entry_idx].choices_height + 5,
          choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].xpos - 10,
          choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].ypos - 10);
        page.put_highlight(
          ((current_entry_idx == entry_count - 1) ? last_choices_width : all_choices_width) + 22,
          entries_info[current_entry_idx].choices_height + 7,
          choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].xpos - 11,
          choice_loc[entries_info[current_entry_idx].first_choice_loc_idx].ypos - 11);
      }
    }
    else {
      if (choice_idx != old_choice_idx) {
        entries_info[current_entry_idx].choice_idx = choice_idx;

        page.clear_highlight(
          entries[current_entry_idx].entry_type == HORIZONTAL_CHOICES ? choice_loc[old_choice_idx].width + 10 : all_choices_width + 10,
          choice_loc[old_choice_idx].height - 5,
          choice_loc[old_choice_idx].xpos - 5,
          choice_loc[old_choice_idx].ypos - 5);

        page.put_highlight(
          entries[current_entry_idx].entry_type == HORIZONTAL_CHOICES ? choice_loc[choice_idx].width + 10 : all_choices_width + 10,
          choice_loc[choice_idx].height - 5,
          choice_loc[choice_idx].xpos - 5,
          choice_loc[choice_idx].ypos - 5);
      }
    }

    page.paint(false);
  }
  else {
    for (int i = 0; i < entry_count; i++) {
      int8_t k = entries_info[i].choice_idx - entries_info[i].first_choice_loc_idx;
      *entries[i].value = entries[i].choices[k].value;
    }
    page.clear_region(
      Screen::WIDTH - 40, 
      Screen::HEIGHT - fmt.screen_bottom - fmt.screen_top,
      20, TOP_YPOS);

    page.paint(false);
  }
  return completed;
}
