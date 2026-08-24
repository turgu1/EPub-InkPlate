// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FORM_VIEWER__ 1
#include "viewers/form_viewer.hpp"

#include "platform.hpp"

#include <iterator>

HimemPool<FormChoiceField::Item> FormChoiceField::itemPool;
uint8_t                          FormChoiceField::fontChoicesCount = 0;

FormChoice FormChoiceField::fontChoices[8] = {
  { nullptr, 0 }, { nullptr, 1 }, { nullptr, 2 },
  { nullptr, 3 }, { nullptr, 4 }, { nullptr, 5 },
  { nullptr, 6 }, { nullptr, 7 } };

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
 *       no field is initially highlighted., int8_t thickness
 *
 * @see FormField, FieldFactory, Page, ScreenBottom
 */
void FormViewer::show(const char *titleArg, FormEntries formEntriesArg, int8_t sizeArg,
                      const char *bottomMsgArg, UpdateType updateType) {

  if (FormChoiceField::fontChoicesCount == 0) { FormChoiceField::adjustFontChoices(); }

  switch (updateType) {
    case UpdateType::INITIAL:
      title       = titleArg;
      formEntries = formEntriesArg;
      size        = sizeArg;
      bottomMsg   = bottomMsgArg;

      computeDisplayParameters();
      generateFields();
      computeFieldsPos();

      #if TOUCH_ENABLE
        oldField = currentField = fields.end();
      #else
        currentField = fields.begin();
        oldField = fields.end();
        (*currentField)->updateState(FormField::State::HIGHLIGHTED, fmt);
      #endif

      displayForm();
      break;

    case UpdateType::PAGE_CHANGE:
      generateFields();
      computeFieldsPos();
      displayForm();
      break;

    case UpdateType::REFRESH:
      displayForm();
      break;
  }

}

auto FormViewer::computeDisplayParameters() -> void {

  uint16_t totalVSpace = screen.getHeight() - BOTTOM_YPOS;

  Glyph * glyph   = font->getGlyph('M', FORM_FONT_SIZE);
  glyphYOffset    = -glyph->yoff;
  lineHeight      = glyph->dim.height;

  // Compute title required space

  titleFontSize = FORM_FONT_SIZE + 4;

  while (true) {
    titleDim = font->getASCIISize(title, fmt.fontSize);
    if (titleDim.width > (Screen::getWidth() - 20)) {
      fmt.fontSize -= 1;
    } else {
      break;
    }
  }

  titleYSpace = (titleDim.height << 1) + 20;

  // Compute bottomMsg required space

  if (bottomMsg != nullptr) {
    auto dim = font->getASCIISize(bottomMsg, FORM_FONT_SIZE);
    bottomMsgYSpace = ((dim.width > screen.getWidth() - 40) ? (dim.height << 1) : dim.height) + 20;
  }

  #if TOUCH_ENABLE
    doneButtonYSpace = lineHeight + 30;
    spaceAvailableForFields = totalVSpace - titleYSpace - bottomMsgYSpace - doneButtonYSpace - 20;
  #else
    spaceAvailableForFields = totalVSpace - titleYSpace - bottomMsgYSpace - 20;
  #endif

  fieldsPerPage = spaceAvailableForFields / (lineHeight + 20);

  #if TOUCH_ENABLE
    if (fieldsPerPage > (size - 1)) fieldsPerPage = size - 1;
  #else
    if (fieldsPerPage > size) fieldsPerPage = size;
  #endif

  generateFields(true);

  maxFieldsWidth = 0;

  for (auto &field : fields) {
    width = field->getFieldDim().width + field->getCaptionDim().width;;
    if (width > maxFieldsWidth) { maxFieldsWidth = width; }
  }

  uint16_t width = maxFieldsWidth + 25;
  rightXpos = (Screen::getWidth() >> 1) + (width >> 1);
}

auto FormViewer::generateFields(bool all) -> void {

  fields.clear();

  for (int i = 0; i < size; ++i) {
    FormFieldPtr field = FieldFactory::create(formEntries[i], *font.get(), *page.get());
    if (field != nullptr) {
      field->computeCaptionDim(lineHeight);
      field->computeDim(lineHeight);
      LOG_I("Field dimentions: Caption: [{}, {}] Field: [{}, {}]", 
            field->getCaptionDim().width, field->getCaptionDim().height, 
            field->getFieldDim().width,   field->getFieldDim().height);
      fields.push_back(std::move(field));
    }
  }
}

auto FormViewer::computeFieldsPos() -> void {

  uint16_t currentYpos  = titleYSpace + 20;
  uint16_t captionLeft = rightXpos - width;

  firstFieldInPage = fields.begin();
  auto endField = fields.begin();
  std::advance(endField, fieldsPerPage);

  for (auto &field = firstFieldInPage; field != endField; ++field) {
    (*field)->computeCaptionPos(Pos(captionLeft, currentYpos), glyphYOffset);
    (*field)->computePos(Pos(rightXpos, currentYpos));
    currentYpos += (*field)->getFieldDim().height + 20;
    LOG_D("Field positions: Caption: [{}, {}] Field: [{}, {}]", (*field)->getCaptionPos().x,
          (*field)->getCaptionPos().y, (*field)->getFieldPos().x, (*field)->getFieldPos().y);
  }
}

auto FormViewer::displayForm() -> void {

  fmt = {
    .fontSize     = FORM_FONT_SIZE + 4,
    .marginLeft   = 5,
    .marginRight  = 5,
    .screenLeft   = 20,
    .screenRight  = 20,
    .screenTop    = titleYSpace,
    .screenBottom = BOTTOM_YPOS,
  };

  page->start(fmt);

  // Display the Title

  page->putRounded(Dim(Screen::getWidth() - 20, titleYSpace - 20), Pos(10, 10), 2);

  page->putStrAt(
    title,
    Pos((Screen::getWidth() - titleDim.width) >> 1, (titleYSpace >> 1) + (titleDim.height >> 1)),
    fmt);

  fmt.fontSize = FORM_FONT_SIZE;

  // The large rectangle into which the form will be drawn

  page->putRounded(
    Dim(Screen::getWidth() - 20, Screen::getHeight() - fmt.screenBottom - fmt.screenTop - 4),
    Pos(10, titleYSpace + 2), 2);

  // Show all captions (but the last one (OK / CANCEL) or (DONE)) and choices

  auto endField = fields.begin();
  std::advance(endField, fieldsPerPage);

  for (auto &field = firstFieldInPage; field != endField; ++field) {
    (*field)->paintCaption(fmt);
    (*field)->updateState(FormField::State::NORMAL, fmt);
  }

  if (bottomMsg != nullptr) {
    auto myFmt = fmt;
    myFmt.screenBottom = BOTTOM_YPOS + 10;
    myFmt.screenTop = screen.getHeight() - myFmt.screenBottom - (bottomMsgYSpace - 20);
    myFmt.align = HAlign::CENTER;

    page->setLimits(myFmt);
    page->newParagraph(myFmt);
    page->addText(bottomMsg, myFmt);
    page->endParagraph(myFmt);
  }

  ScreenBottom::show(page);

  page->paint(true);
}


auto FormViewer::event(const EventMgr::Event &event) -> bool {

  LOG_I("Event...");
  EventMgr::Event eventCopy = event; // Create a mutable copy of the event to allow modifications

  completed = false;
  page->start(fmt);


  #if TOUCH_ENABLE
    if ((eventCopy.kind == EventMgr::EventKind::TAP) && 
        (currentField != fields.end()) && 
        ((*currentField)->getState() == FormField::State::SELECTED) && 
        ((*currentField)->inEventControl())) {
      if (!(*currentField)->event(eventCopy, fmt)) {
        LOG_I("Field released control of events, resetting to NORMAL state...");
        if ((*currentField)->formRefreshRequired()) {
          show(title, formEntries, size, bottomMsg, UpdateType::REFRESH);
          page->start(fmt);
          (*currentField)->updateState(FormField::State::SELECTED, fmt);
        }
      } else {
        LOG_I("Field took control of events, keeping it in SELECTED state...");
      }
    } else {
      auto foundField = findField(eventCopy.x, eventCopy.y);

      if (foundField != fields.end()) {
        oldField = currentField;
        currentField = foundField;

        if ((eventCopy.kind == EventMgr::EventKind::TAP) && 
            (currentField != fields.end()) && 
            ((*currentField)->getState() == FormField::State::SELECTED)) {
          if (!(*currentField)->event(eventCopy, fmt)) {
            LOG_I("Field released control of events, resetting to NORMAL state...");
            (*currentField)->updateState(FormField::State::NORMAL, fmt);
            currentField = fields.end();
          } else {
            LOG_I("Field took control of events, keeping it in SELECTED state...");
          }
        } else {
          if (currentField != fields.end()) {
            switch (eventCopy.kind) {
            case EventMgr::EventKind::TAP:
              LOG_I("TAP: Field found at position [{}, {}]", eventCopy.x, eventCopy.y);
              if ((*currentField)->getType() == FormEntryType::DONE) {
                completed = true;
              } else {
                if ((*currentField)->getState() != FormField::State::SELECTED) {
                  LOG_I("new field to SELECTED state...");
                  (*currentField)->updateState(FormField::State::SELECTED, fmt);
                }
                if ((oldField != fields.end()) && (oldField != currentField)) {
                  LOG_I("old field to NORMAL state...");
                  (*oldField)->updateState(FormField::State::NORMAL, fmt);
                }
              }
              break;

            default:
              break;
            }
          } else {
            LOG_I("Field not found at position [{}, {}]", eventCopy.x, eventCopy.y);
            for (auto &field : fields) {
              LOG_I("Field: [{}, {}] .. [{}, {}]", 
                    field->getFieldPos().x, field->getFieldPos().y,
                    field->getFieldPos().x + field->getFieldDim().width, 
                    field->getFieldPos().y + field->getFieldDim().height);
            }
            LOG_I("-------------------------------");
          }
        }
      }
    }
  #else
    oldField = currentField;
    if ((*currentField)->getState() == FormField::State::SELECTED) {
      if (!(*currentField)->event(eventCopy, fmt)) {
        (*currentField)->updateState(FormField::State::HIGHLIGHTED, fmt);
        if ((*currentField)->formRefreshRequired()) {
          show(title, formEntries, size, bottomMsg, UpdateType::REFRESH);
          page->start(fmt);
          (*currentField)->updateState(FormField::State::SELECTED, fmt);
        }
        switch (eventCopy.kind) {
        case EventMgr::EventKind::SELECT:
        case EventMgr::EventKind::DBL_NEXT:
          ++currentField;
          if (currentField == fields.end()) { currentField = fields.begin(); }
          (*oldField)->updateState(FormField::State::NORMAL, fmt);
          (*currentField)->updateState(FormField::State::HIGHLIGHTED, fmt);
          break;
        case EventMgr::EventKind::DBL_PREV:
          if (currentField == fields.begin()) { currentField = fields.end(); }
          --currentField;
          (*oldField)->updateState(FormField::State::NORMAL, fmt);
          (*currentField)->updateState(FormField::State::HIGHLIGHTED, fmt);
          break;
        case EventMgr::EventKind::DBL_SELECT:
          completed = true;
          break;
        default: 
          break;
        }
      }
    } else {
      Fields::iterator oldField = currentField;
      switch (eventCopy.kind) {
      case EventMgr::EventKind::DBL_PREV:
        if (currentField == fields.begin()) { currentField = fields.end(); }
        --currentField;
        (*oldField)->updateState(FormField::State::NORMAL, fmt);
        (*currentField)->updateState(FormField::State::HIGHLIGHTED, fmt);
        break;
      case EventMgr::EventKind::DBL_NEXT:
        ++currentField;
        if (currentField == fields.end()) { currentField = fields.begin(); }
        (*oldField)->updateState(FormField::State::NORMAL, fmt);
        (*currentField)->updateState(FormField::State::HIGHLIGHTED, fmt);
        break;
      case EventMgr::EventKind::SELECT:
        (*currentField)->updateState(FormField::State::SELECTED, fmt);
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

  if (completed) {
    for (auto &field : fields) {
      field->saveValue();
    }
    fields.clear();

  }  else {
    ScreenBottom::show(page);

    page->paint(false);
  }

  return completed;
}