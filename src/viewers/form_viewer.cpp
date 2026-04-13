// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FORM_VIEWER__ 1
#include "viewers/form_viewer.hpp"

MemoryPool<FormChoiceField::Item> FormChoiceField::item_pool;
uint8_t FormChoiceField::font_choices_count = 0;

FormChoice FormChoiceField::font_choices[8] = {{nullptr, 0}, {nullptr, 1}, {nullptr, 2},
                                               {nullptr, 3}, {nullptr, 4}, {nullptr, 5},
                                               {nullptr, 6}, {nullptr, 7}};

// #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
//   bool FormDone::event(const EventMgr::Event &event) {
//     form_viewer.set_completed(true);
//     return false;
//   }
// #endif

/**
 * @brief Displays a form with the given entries and optional message at the bottom.
 *
 * This method renders a form interface with multiple input fields. On the first call
 * (when refresh=false), it initializes the form by creating field objects, calculating
 * their dimensions, and computing their positions on the screen. Subsequent calls with
 * refresh=true will redraw the form without reinitializing the fields.
 *
 * The form is centered horizontally on the screen with automatic spacing between fields.
 * Each field displays a caption and an input area. The method also displays a message
 * at the bottom of the form and manages field highlighting/selection based on the platform.
 *
 * @param _form_entries Array of FormEntry objects containing the form field definitions
 * @param _size Number of entries in the form (number of form fields)
 * @param _bottom_msg Pointer to a C-string containing the message to display at the bottom
 *                     of the form (typically navigation instructions like "OK/CANCEL")
 * @param refresh Boolean flag indicating whether to refresh the display without reinitializing.
 *                If false (default), reinitializes form fields and calculates
 * dimensions/positions. If true, redraws the existing form without recalculation.
 *
 * @note On non-touchscreen platforms, the first field is automatically highlighted.
 *       On touchscreen-enabled platforms (INKPLATE_6PLUS, INKPLATE_6FLICK, etc.),
 *       no field is initially highlighted.
 *
 * @see FormField, FieldFactory, Page, ScreenBottom
 */
void FormViewer::show(const char *_title, FormEntries _form_entries, int8_t _size,
                      const char *_bottom_msg, bool refresh) {

  Font *font = fonts.get(1);

  if (!refresh) {
    title        = _title;
    form_entries = _form_entries;
    size         = _size;
    bottom_msg   = _bottom_msg;

    fields.clear();

    for (int i = 0; i < size; i++) {
      FormFieldPtr field = FieldFactory::create(form_entries[i], *font, *page.get());
      if (field != nullptr) {
        field->compute_caption_dim();
        field->compute_field_dim();
        LOG_D("Field dimentions: Caption: [%d, %d] Field: [%d, %d]", field->get_caption_dim().width,
              field->get_caption_dim().height, field->get_field_dim().width,
              field->get_field_dim().height);
        fields.push_back(std::move(field));
      }
    }

    all_fields_width = all_captions_width = 0;

    int16_t width;

    for (auto &field : fields) {
      width = field->get_field_dim().width;
      if (width > all_fields_width) all_fields_width = width;

      width = field->get_caption_dim().width;
      if (width > all_captions_width) all_captions_width = width;
    }

    width                    = all_captions_width + all_fields_width + 25;
    const int16_t right_xpos = (Screen::get_width() >> 1) + (width >> 1);

    int16_t current_ypos  = TOP_YPOS + 20;
    int16_t caption_right = right_xpos - all_fields_width - 25;
    int16_t field_left    = right_xpos - all_fields_width - 10;

    for (auto &field : fields) {
      field->compute_caption_pos(Pos(caption_right, current_ypos));
      field->compute_field_pos(Pos(field_left, current_ypos));
      current_ypos += field->get_field_dim().height + 20;
      LOG_D("Field positions: Caption: [%d, %d] Field: [%d, %d]", field->get_caption_pos().x,
            field->get_caption_pos().y, field->get_field_pos().x, field->get_field_pos().y);
    }

    bottom_msg_pos = Pos(40, current_ypos + 30);
  }

  // Display the form

  Page::Format fmt = {
      .font_size     = FORM_FONT_SIZE + 4,
      .margin_left   = 5,
      .margin_right  = 5,
      .screen_left   = 20,
      .screen_right  = 20,
      .screen_top    = TOP_YPOS,
      .screen_bottom = BOTTOM_YPOS,
  };

  page->start(fmt);

  page->put_rounded(Dim(Screen::get_width() - 44, TOP_YPOS - 20), Pos(22, 10));

  Dim title_dim;

  font->get_size(title, &title_dim, FORM_FONT_SIZE + 4);

  page->put_str_at(
      title,
      Pos((Screen::get_width() - title_dim.width) >> 1, (TOP_YPOS >> 1) + (title_dim.height >> 1)),
      fmt);

  // Page::Format fmt = {
  //     .font_size     = FORM_FONT_SIZE,
  //     .margin_left   = 5,
  //     .margin_right  = 5,
  //     .screen_left   = 20,
  //     .screen_right  = 20,
  //     .screen_top    = TOP_YPOS,
  //     .screen_bottom = BOTTOM_YPOS,
  // };

  fmt.font_size = FORM_FONT_SIZE;

  // The large rectangle into which the form will be drawn

  // page.clear_region(
  //     Dim(Screen::get_width() - 40, Screen::get_height() - fmt.screen_bottom - fmt.screen_top),
  //     Pos(20, TOP_YPOS));

  page->put_rounded(
      Dim(Screen::get_width() - 44, Screen::get_height() - fmt.screen_bottom - fmt.screen_top - 4),
      Pos(22, TOP_YPOS + 2));

  // Show all captions (but the last one (OK / CANCEL) or (DONE)) and choices

  for (auto &field : fields) {
    field->paint(fmt);
    field->update_highlight();
  }

  page->put_str_at(bottom_msg, bottom_msg_pos, fmt);

  if (!refresh) {
    selecting_field = false;

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      current_field      = fields.end();
      highlighting_field = false;
    #else
      current_field      = fields.begin();
      highlighting_field = true;
      (*current_field)->show_highlighted(true);
    #endif
  }

  ScreenBottom::show(page);

  page->paint(true);
}

bool FormViewer::event(const EventMgr::Event &event) {

  completed = false;

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    if (current_field != fields.end()) {
      if (!(*current_field)->event(event)) {
        // The field releases control of future events
        // Redraw the form
        show(title, form_entries, size, bottom_msg, true);
        current_field = fields.end();
      }
      return false; // Not completed yet
    } else {
      switch (event.kind) {
      case EventMgr::EventKind::TAP:
        current_field = find_field(event.x, event.y);

        if ((*current_field)->get_type() == FormEntryType::DONE) {
          completed = true;
        } else {
          if (current_field != fields.end()) {
            if ((*current_field)->event(event)) {
              // The field needs to keep control of future events
              // current_field is not reset for next event loop to pass
              // control to it.
              return false;
            } else {
              // The field doesn't need control of future events
              current_field = fields.end();
            }
          }
        }
        break;

      default:
        break;
      }
    }
  #else
    Fields::iterator old_field = current_field;
    if (highlighting_field) {
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
        highlighting_field = false;
        selecting_field = true;
        break;
      case EventMgr::EventKind::NONE:
        return false;
      case EventMgr::EventKind::DBL_SELECT:
        completed = true;
        break;
      }
    } else {
      bool was_in_control = (*current_field)->in_event_control();
      if (!(*current_field)->event(event)) {
        if ((*current_field)->form_refresh_required()) {
          show(title, form_entries, size, bottom_msg, true);
        }
        switch (event.kind) {
        case EventMgr::EventKind::SELECT:
          highlighting_field = true;
          old_field = current_field;
          current_field++;
          if (current_field == fields.end()) current_field = fields.begin();
          break;
        case EventMgr::EventKind::NONE:
          return false;
        case EventMgr::EventKind::DBL_SELECT:
          if (!was_in_control)
            completed = true;
          else {
            highlighting_field = true;
            old_field = current_field;
            current_field++;
            if (current_field == fields.end()) current_field = fields.begin();
          }
          break;
        default:
          break;
        }
      }
    }
  #endif

  if (completed) {
    for (auto &field : fields) {
      field->save_value();
    }
    fields.clear();

  } else {
    Page::Format fmt = {
        .font_size     = FORM_FONT_SIZE,
        .screen_left   = 20,
        .screen_right  = 20,
        .screen_top    = TOP_YPOS,
        .screen_bottom = BOTTOM_YPOS,
    };

    page->start(fmt);

    if (highlighting_field) {
      #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
        (*old_field)->show_selected(false);
        (*current_field)->show_highlighted(true);
      #endif
    } else {
      #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
        if (selecting_field) {
          selecting_field = false;
          (*current_field)->show_selected(true);
          (*current_field)->event(event);
        }
      #endif

      for (auto &field : fields) {
        field->update_highlight();
      }
    }

    ScreenBottom::show(page);

    page->paint(false);
  }

  return completed;
}