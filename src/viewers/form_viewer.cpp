// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FORM_VIEWER__ 1
#include "viewers/form_viewer.hpp"

MemoryPool<FormChoiceField::Item> FormChoiceField::itemPool;
uint8_t FormChoiceField::fontChoicesCount = 0;

// clang-format off

FormChoice FormChoiceField::fontChoices[8] = {
  {nullptr, 0}, {nullptr, 1}, {nullptr, 2}, {nullptr, 3}, 
  {nullptr, 4}, {nullptr, 5}, {nullptr, 6}, {nullptr, 7}};

// clang-format on

// #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
//   bool FormDone::event(const EventMgr::Event &event) {
//     form_viewer.setCompleted(true);
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
 * @param formEntriesArg Array of FormEntry objects containing the form field definitions
 * @param sizeArg Number of entries in the form (number of form fields)
 * @param bottomMsgArg Pointer to a C-string containing the message to display at the bottom
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
void FormViewer::show(const char *titleArg, FormEntries formEntriesArg, int8_t sizeArg,
                      const char *bottomMsgArg, bool refresh) {

  if (FormChoiceField::fontChoicesCount == 0) FormChoiceField::adjustFontChoices();

  FontPtr &font = appFonts.getFont(1);

  if (!refresh) {
    title       = titleArg;
    formEntries = formEntriesArg;
    size        = sizeArg;
    bottomMsg   = bottomMsgArg;

    fields.clear();

    for (int i = 0; i < size; i++) {
      FormFieldPtr field = FieldFactory::create(formEntries[i], *font.get(), *page.get());
      if (field != nullptr) {
        field->computeCaptionDim();
        field->computeFieldDim();
        LOG_D("Field dimentions: Caption: [%d, %d] Field: [%d, %d]", field->getCaptionDim().width,
              field->getCaptionDim().height, field->getFieldDim().width,
              field->getFieldDim().height);
        fields.push_back(std::move(field));
      }
    }

    allFieldsWidth = allCaptionsWidth = 0;

    int16_t width;

    for (auto &field : fields) {
      width = field->getFieldDim().width;
      if (width > allFieldsWidth) allFieldsWidth = width;

      width = field->getCaptionDim().width;
      if (width > allCaptionsWidth) allCaptionsWidth = width;
    }

    width                   = allCaptionsWidth + allFieldsWidth + 25;
    const int16_t rightXpos = (Screen::getWidth() >> 1) + (width >> 1);

    int16_t currentYpos  = TOP_YPOS + 20;
    int16_t captionRight = rightXpos - allFieldsWidth - 25;
    int16_t fieldLeft    = rightXpos - allFieldsWidth - 10;

    for (auto &field : fields) {
      field->computeCaptionPos(Pos(captionRight, currentYpos));
      field->computeFieldPos(Pos(fieldLeft, currentYpos));
      currentYpos += field->getFieldDim().height + 20;
      LOG_D("Field positions: Caption: [%d, %d] Field: [%d, %d]", field->getCaptionPos().x,
            field->getCaptionPos().y, field->getFieldPos().x, field->getFieldPos().y);
    }

    bottomMsgPos = Pos(40, currentYpos + 30);
  }

  // Display the form

  Page::Format fmt = {
      .fontSize     = FORM_FONT_SIZE + 4,
      .marginLeft   = 5,
      .marginRight  = 5,
      .screenLeft   = 20,
      .screenRight  = 20,
      .screenTop    = TOP_YPOS,
      .screenBottom = BOTTOM_YPOS,
  };

  page->start(fmt);

  page->putRounded(Dim(Screen::getWidth() - 44, TOP_YPOS - 20), Pos(22, 10));

  Dim titleDim;

  font->getSize(title, &titleDim, FORM_FONT_SIZE + 4);

  page->putStrAt(
      title,
      Pos((Screen::getWidth() - titleDim.width) >> 1, (TOP_YPOS >> 1) + (titleDim.height >> 1)),
      fmt);

  // Page::Format fmt = {
  //     .fontSize     = FORM_FONT_SIZE,
  //     .marginLeft   = 5,
  //     .marginRight  = 5,
  //     .screenLeft   = 20,
  //     .screenRight  = 20,
  //     .screenTop    = TOP_YPOS,
  //     .screenBottom = BOTTOM_YPOS,
  // };

  fmt.fontSize = FORM_FONT_SIZE;

  // The large rectangle into which the form will be drawn

  // page.clearRegion(
  //     Dim(Screen::getWidth() - 40, Screen::getHeight() - fmt.screenBottom - fmt.screenTop),
  //     Pos(20, TOP_YPOS));

  page->putRounded(
      Dim(Screen::getWidth() - 44, Screen::getHeight() - fmt.screenBottom - fmt.screenTop - 4),
      Pos(22, TOP_YPOS + 2));

  // Show all captions (but the last one (OK / CANCEL) or (DONE)) and choices

  for (auto &field : fields) {
    field->paint(fmt);
    field->updateHighlight();
  }

  page->putStrAt(bottomMsg, bottomMsgPos, fmt);

  if (!refresh) {
    selectingField = false;

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      currentField      = fields.end();
      highlightingField = false;
    #else
      currentField      = fields.begin();
      highlightingField = true;
      (*currentField)->showHighlighted(true);
    #endif
  }

  ScreenBottom::show(page);

  page->paint(true);
}

auto FormViewer::event(const EventMgr::Event &event) -> bool {

  completed = false;

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    if (currentField != fields.end()) {
      if (!(*currentField)->event(event)) {
        // The field releases control of future events
        // Redraw the form
        show(title, formEntries, size, bottomMsg, true);
        currentField = fields.end();
      }
      return false; // Not completed yet
    } else {
      switch (event.kind) {
      case EventMgr::EventKind::TAP:
        currentField = findField(event.x, event.y);

        if ((currentField != fields.end()) && ((*currentField)->getType() == FormEntryType::DONE)) {
          completed = true;
        } else {
          if (currentField != fields.end()) {
            if ((*currentField)->event(event)) {
              // The field needs to keep control of future events
              // currentField is not reset for next event loop to pass
              // control to it.
              return false;
            } else {
              // The field doesn't need control of future events
              currentField = fields.end();
            }
          }
        }
        break;

      default:
        break;
      }
    }
  #else
    Fields::iterator oldField = currentField;
    if (highlightingField) {
      switch (event.kind) {
      case EventMgr::EventKind::DBL_PREV:
      case EventMgr::EventKind::PREV:
        if (currentField == fields.begin()) currentField = fields.end();
        currentField--;
        break;
      case EventMgr::EventKind::DBL_NEXT:
      case EventMgr::EventKind::NEXT:
        currentField++;
        if (currentField == fields.end()) currentField = fields.begin();
        break;
      case EventMgr::EventKind::SELECT:
        highlightingField = false;
        selectingField = true;
        break;
      case EventMgr::EventKind::NONE:
        return false;
      case EventMgr::EventKind::DBL_SELECT:
        completed = true;
        break;
      }
    } else {
      bool wasInControl = (*currentField)->inEventControl();
      if (!(*currentField)->event(event)) {
        if ((*currentField)->formRefreshRequired()) {
          show(title, formEntries, size, bottomMsg, true);
        }
        switch (event.kind) {
        case EventMgr::EventKind::SELECT:
          highlightingField = true;
          oldField = currentField;
          currentField++;
          if (currentField == fields.end()) currentField = fields.begin();
          break;
        case EventMgr::EventKind::NONE:
          return false;
        case EventMgr::EventKind::DBL_SELECT:
          if (!wasInControl)
            completed = true;
          else {
            highlightingField = true;
            oldField = currentField;
            currentField++;
            if (currentField == fields.end()) currentField = fields.begin();
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
      field->saveValue();
    }
    fields.clear();

  } else {
    Page::Format fmt = {
        .fontSize     = FORM_FONT_SIZE,
        .screenLeft   = 20,
        .screenRight  = 20,
        .screenTop    = TOP_YPOS,
        .screenBottom = BOTTOM_YPOS,
    };

    page->start(fmt);

    if (highlightingField) {
      #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
        (*oldField)->showSelected(false);
        (*currentField)->showHighlighted(true);
      #endif
    } else {
      #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
        if (selectingField) {
          selectingField = false;
          (*currentField)->showSelected(true);
          (*currentField)->event(event);
        }
      #endif

      for (auto &field : fields) {
        field->updateHighlight();
      }
    }

    ScreenBottom::show(page);

    page->paint(false);
  }

  return completed;
}