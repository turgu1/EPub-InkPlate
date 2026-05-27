// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"
#include "himem_pool.hpp"

#include "config.hpp"
#include "controllers/event_mgr.hpp"
#include "fonts.hpp"
#include "fonts_db.hpp"
#include "viewers/keypad_viewer.hpp"
#include "viewers/page.hpp"
#include "viewers/screen_bottom.hpp"

#include <list>

enum class FormEntryType {
  HORIZONTAL,
  VERTICAL,
  UINT16,
  FLOAT,
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL || TOUCH_MENU
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
      int8_t choiceCount;
      const FormChoice *choices;
    } ch;
    struct {
      uint16_t *value;
      uint16_t min;
      uint16_t max;
    } intVal;
    struct {
      double *value;
      double min;
      double max;
    } floatVal;
  } u;
  FormEntryType entryType;
};

using FormFieldPtr = HimemUniquePtr<class FormField>;
class FormField {
private:
  static constexpr char const *TAG = "FormField";

protected:
  FormEntry &formEntry;
  Font &font;
  Page &page;
  bool eventControl;
  Dim fieldDim, captionDim;
  Pos fieldPos, captionPos;

public:
  FormField(FormEntry &formEntry, Font &font, Page &page)
      : formEntry(formEntry), font(font), page(page), eventControl(false) {};
  virtual ~FormField() = default; // { LOG_I("FormField destructor called"); }

  virtual auto getFieldDim() -> const Dim { return fieldDim; }
  [[nodiscard]] inline auto getCaptionDim() -> const Dim & { return captionDim; }

  [[nodiscard]] inline auto getFieldPos() -> const Pos & { return fieldPos; }
  [[nodiscard]] inline auto getCaptionPos() -> const Pos & { return captionPos; }

  [[nodiscard]] inline auto getType() -> FormEntryType { return formEntry.entryType; }

  auto computeCaptionDim() -> void {
    if (formEntry.caption != nullptr) {
      font.getASCIISize(formEntry.caption, &captionDim, FORM_FONT_SIZE);
    } else {
      captionDim = Dim(0, 0);
    }
  }

  virtual auto formRefreshRequired() -> bool { return false; }
  virtual auto paint(Page::Format &fmt) -> void     = 0;
  virtual auto computeFieldDim() -> void            = 0;
  virtual auto computeFieldPos(Pos fromPos) -> void = 0;
  virtual auto updateHighlight() -> void            = 0;
  virtual auto saveValue() -> void                  = 0;

  virtual auto event(const EventMgr::Event &event) -> bool { return false; }

  [[nodiscard]] inline auto inEventControl() -> bool { return eventControl; }

  auto computeCaptionPos(Pos fromPos) -> void {
    captionPos = Pos(fromPos.x - captionDim.width, fromPos.y);
  }

  auto showHighlighted(bool showIt) -> void {
    if (showIt) {
      page.putHighlight(Dim(fieldDim.width + 20, fieldDim.height + 20),
                        Pos(fieldPos.x - 10, fieldPos.y - 10));
    } else {
      page.clearHighlight(Dim(fieldDim.width + 20, fieldDim.height + 20),
                          Pos(fieldPos.x - 10, fieldPos.y - 10));
    }
  }

  virtual auto showSelected(bool showIt) -> void {
    if (showIt) {
      page.putHighlight(Dim(fieldDim.width + 20, fieldDim.height + 20),
                        Pos(fieldPos.x - 10, fieldPos.y - 10));
      page.putHighlight(Dim(fieldDim.width + 22, fieldDim.height + 22),
                        Pos(fieldPos.x - 11, fieldPos.y - 11));
      page.putHighlight(Dim(fieldDim.width + 24, fieldDim.height + 24),
                        Pos(fieldPos.x - 12, fieldPos.y - 12));
    } else {
      page.clearHighlight(Dim(fieldDim.width + 20, fieldDim.height + 20),
                          Pos(fieldPos.x - 10, fieldPos.y - 10));
      page.clearHighlight(Dim(fieldDim.width + 22, fieldDim.height + 22),
                          Pos(fieldPos.x - 11, fieldPos.y - 11));
      page.clearHighlight(Dim(fieldDim.width + 24, fieldDim.height + 24),
                          Pos(fieldPos.x - 12, fieldPos.y - 12));
    }
  }

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL || TOUCH_MENU
  [[nodiscard]] inline auto isPointed(uint16_t x, uint16_t y) -> bool {
    return (x >= (fieldPos.x - 10)) && (y >= (fieldPos.y - 10)) &&
           (x <= (fieldPos.x + fieldDim.width + 10)) && (y <= (fieldPos.y + fieldDim.height + 10));
  }

#endif
};

class FormChoiceField : public FormField {
private:
  constexpr static char const *TAG = "FormChoiceField";

public:
  static constexpr FormChoice dirViewChoices[2] = {{"LINEAR", 0}, {"MATRIX", 1}};

  static constexpr FormChoice coverSizeChoices[3] = {{"SMALL", 0}, {"MEDIUM", 1}, {"LARGE", 2}};

  static constexpr FormChoice lineHeightChoices[3] = {{"TIGHT", 0}, {"MEDIUM", 1}, {"LARGE", 2}};

  static constexpr FormChoice okCancelChoices[2] = {{"OK", 1}, {"CANCEL", 0}};

  static constexpr FormChoice yesNoChoices[2] = {{"YES", 1}, {"NO", 0}};

  static constexpr FormChoice resolutionChoices[2] = {{"1Bit", 0}, {"3Bits", 1}};

  static constexpr FormChoice timeoutChoices[3] = {{"5", 5}, {"15", 15}, {"30", 30}};

  static constexpr FormChoice batteryVisualChoices[4] = {
      {"NONE", 0}, {"PERCENT", 1}, {"VOLTAGE", 2}, {"ICON", 3}};

  static constexpr FormChoice fontSizeChoices[4] = {{"8", 8}, {"10", 10}, {"12", 12}, {"15", 15}};

#if DATE_TIME_RTC
  static constexpr FormChoice rightCornerChoices[3] = {{"NONE", 0}, {"DATE TIME", 1}, {"HEAP", 2}};
#endif

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL || TOUCH_MENU
  static constexpr FormChoice orientationChoices[4] = {
      {"LEFT", 3}, {"RIGHT", 2}, {"TOP", 1}, {"BOTTOM", 0}};
#else
  static constexpr FormChoice orientationChoices[3] = {{"LEFT", 0}, {"RIGHT", 1}, {"BOTTOM", 2}};
#endif

  static FormChoice fontChoices[8];
  static uint8_t fontChoicesCount;

  auto computeFieldDim() -> void {
    fieldDim = {0, 0};
    for (int8_t i = 0; i < formEntry.u.ch.choiceCount; ++i) {
      Item *item = itemPool.newElement();
      items.push_back(item);
      font.getASCIISize(formEntry.u.ch.choices[i].caption, &item->dim, FORM_FONT_SIZE);
      item->idx = i;
    }

    int i = 0;
    for (Items::iterator it = items.begin(); it != items.end(); ++it) {
      if (formEntry.u.ch.choices[i].value == *formEntry.u.ch.value) {
        currentItem = it;
        break;
      }
      ++i;
    }
    if (currentItem == items.end()) currentItem = items.begin();

    oldItem = items.end();
  }

  static auto adjustFontChoices() -> void {
    FontsDB *fontsDB{nullptr};

    config.get(Config::Ident::FONTS_DB, &fontsDB);
    auto fontCount = fontsDB ? fontsDB->getStandardFontCount() : 0;
    if (fontCount > 8) fontCount = 8;
    for (uint8_t i = 0; i < fontCount; ++i) {
      fontChoices[i].caption = fontsDB->getStandardFontName(i).c_str();
    }
    fontChoicesCount = fontCount;
  }

  auto computeFieldPos(Pos fromPos) -> void = 0;

  auto paint(Page::Format &fmt) -> void {

    Glyph *glyph   = font.getGlyph('M', FORM_FONT_SIZE);
    uint8_t offset = -glyph->yoff;

    page.putStrAt(formEntry.caption, Pos(captionPos.x, captionPos.y + offset), fmt);
    for (auto *item : items) {
      page.putStrAt(formEntry.u.ch.choices[item->idx].caption,
                    Pos(item->pos.x, item->pos.y + offset), fmt);
    }
  }

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  auto event(const EventMgr::Event &event) -> bool {
    oldItem = currentItem;
    Items::iterator it;
    for (it = items.begin(); it != items.end(); ++it) {
      if ((event.x >= (*it)->pos.x - 5) && (event.y >= (*it)->pos.y - 5) &&
          (event.x <= ((*it)->pos.x + (*it)->dim.width + 5)) &&
          (event.y <= ((*it)->pos.y + (*it)->dim.height + 5))) {
        break;
      }
    }
    if (it != items.end()) currentItem = it;
    return false;
  }
#else
  auto event(const EventMgr::Event &event) -> bool {
    oldItem = currentItem;
    switch (event.kind) {
    case EventMgr::EventKind::DBL_PREV:
    case EventMgr::EventKind::PREV:
      if (currentItem == items.begin()) currentItem = items.end();
      currentItem--;
      break;
    case EventMgr::EventKind::DBL_NEXT:
    case EventMgr::EventKind::NEXT:
      ++currentItem;
      if (currentItem == items.end()) currentItem = items.begin();
      break;
    default:
      break;
    }
    return false;
  }
#endif

  auto updateHighlight() -> void {
    if (oldItem != currentItem) {
      if (oldItem != items.end()) {
        page.clearHighlight(Dim((*oldItem)->dim.width + 10, (*oldItem)->dim.height + 10),
                            Pos((*oldItem)->pos.x - 5, (*oldItem)->pos.y - 5));
      }
      page.putHighlight(Dim((*currentItem)->dim.width + 10, (*currentItem)->dim.height + 10),
                        Pos((*currentItem)->pos.x - 5, (*currentItem)->pos.y - 5));
    }
    oldItem = currentItem;
  }

  auto saveValue() -> void {
    *formEntry.u.ch.value = formEntry.u.ch.choices[(*currentItem)->idx].value;
  }

protected:
  struct Item {
    Pos pos;
    Dim dim;
    uint8_t idx;
  };

  static HimemPool<Item> itemPool; // Shared by all FormChoiceField

  using Items = std::list<Item *>;

  Items items;
  Items::iterator currentItem, oldItem;

public:
  using FormField::FormField;

  ~FormChoiceField() {
    for (auto *item : items) {
      itemPool.deleteElement(item);
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

  static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
    return makeUniqueHimem<VFormChoiceField>(formEntry, font, page);
  }

  auto computeFieldPos(Pos fromPos) -> void {
    fieldPos   = fromPos;
    Pos thePos = fromPos;

    uint8_t lineHeight = font.getLineHeight(FORM_FONT_SIZE);
    for (auto *item : items) {
      item->pos = thePos;
      thePos.y += lineHeight;
      LOG_D("Item position  [%d, %d]", item->pos.x, item->pos.y);
    }
  }

  auto computeFieldDim() -> void {
    FormChoiceField::computeFieldDim();
    uint8_t lineHeight = font.getLineHeight(FORM_FONT_SIZE);
    uint8_t lastHeight = 0;
    for (auto *item : items) {
      if (fieldDim.width < item->dim.width) fieldDim.width = item->dim.width;
      fieldDim.height += lineHeight;
      lastHeight = item->dim.height;
      LOG_D("Item dimension: [%d, %d]", item->dim.width, item->dim.height);
    }
    fieldDim.height += lastHeight - lineHeight;
  }
};

using HFormChoiceFieldPtr = HimemUniquePtr<class HFormChoiceField>;

class HFormChoiceField : public FormChoiceField {
private:
  static constexpr char const *TAG = "HFormChoiceField";

public:
  using FormChoiceField::FormChoiceField;

  HFormChoiceField()  = default;
  ~HFormChoiceField() = default;

  static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
    return makeUniqueHimem<HFormChoiceField>(formEntry, font, page);
  }

  auto computeFieldPos(Pos fromPos) -> void {
    fieldPos   = fromPos;
    Pos thePos = fromPos;

    for (auto *item : items) {
      item->pos = thePos;
      thePos.x += item->dim.width + 20;
      LOG_D("Item position: [%d, %d]", item->pos.x, item->pos.y);
    }
  }

  static constexpr uint8_t HORIZONTAL_SEPARATOR = 20;

  auto computeFieldDim() -> void {
    FormChoiceField::computeFieldDim();
    uint16_t separator = 0;
    for (auto *item : items) {
      if (fieldDim.height < item->dim.height) fieldDim.height = item->dim.height;
      fieldDim.width += item->dim.width + separator;
      separator = HORIZONTAL_SEPARATOR;
      LOG_D("Item dimension: [%d, %d]", item->dim.width, item->dim.height);
    }
  }
};

using FormUInt16Ptr = HimemUniquePtr<class FormUInt16>;

class FormUInt16 : public FormField {
private:
  KeypadViewerPtr keypadViewer{KeypadViewer::Make()};

public:
  using FormField::FormField;

  FormUInt16()  = default;
  ~FormUInt16() = default;

  static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
    return makeUniqueHimem<FormUInt16>(formEntry, font, page);
  }

  auto showSelected(bool showIt) -> void {

    // As the keypad viewer is used to edit the value of the field, we don't
    // want to show the field as selected when it's a numeric field, to avoid confusion with the
    // highlighting of the keypad viewer. So we just show the field as highlighted when it's
    // selected, and not when it's in control of events.
    if (!showIt) {
      page.clearHighlight(Dim(fieldDim.width + 20, fieldDim.height + 20),
                          Pos(fieldPos.x - 10, fieldPos.y - 10));
    }
  }

  auto formRefreshRequired() -> bool { return true; }

  auto computeFieldPos(Pos fromPos) -> void { fieldPos = fromPos; }

  auto paint(Page::Format &fmt) -> void {
    char val[8];
    Glyph *glyph   = font.getGlyph('M', FORM_FONT_SIZE);
    uint8_t offset = -glyph->yoff;

    uint16_t v = *formEntry.u.intVal.value;
    if (v < formEntry.u.intVal.min) {
      v = formEntry.u.intVal.min;
    } else if (v > formEntry.u.intVal.max) {
      v = formEntry.u.intVal.max;
    }
    *formEntry.u.intVal.value = v;

    int_to_str(v, val, 8);
    page.putStrAt(formEntry.caption, Pos(captionPos.x, captionPos.y + offset), fmt);
    page.putStrAt(val, Pos(fieldPos.x, fieldPos.y + offset), fmt);
  }

  auto event(const EventMgr::Event &event) -> bool {
    if (!eventControl) {
      keypadViewer->show(*formEntry.u.intVal.value, formEntry.caption);
      eventControl = true;
    } else {
      if (!keypadViewer->event(event)) {
        uint16_t v = keypadViewer->getIntValue();
        if (v < formEntry.u.intVal.min) {
          v = formEntry.u.intVal.min;
        } else if (v > formEntry.u.intVal.max) {
          v = formEntry.u.intVal.max;
        }
        *formEntry.u.intVal.value = v;
        eventControl              = false;

        return false; // release events control
      }
    }
    return true; // keep events control
  }

  auto updateHighlight() -> void {}

  auto saveValue() -> void {}

  auto computeFieldDim() -> void { font.getASCIISize("XXXXX", &fieldDim, FORM_FONT_SIZE); }
};

using FormFloatPtr = HimemUniquePtr<class FormFloat>;

class FormFloat : public FormField {
private:
  KeypadViewerPtr keypadViewer{KeypadViewer::Make()};

public:
  using FormField::FormField;

  FormFloat()  = default;
  ~FormFloat() = default;

  static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
    return makeUniqueHimem<FormFloat>(formEntry, font, page);
  }

  auto showSelected(bool showIt) -> void {
    // As the keypad viewer is used to edit the value of the field, we don't
    // want to show the field as selected when it's a numeric field, to avoid confusion with the
    // highlighting of the keypad viewer. So we just show the field as highlighted when it's
    // selected, and not when it's in control of events.
    if (!showIt) {
      page.clearHighlight(Dim(fieldDim.width + 20, fieldDim.height + 20),
                          Pos(fieldPos.x - 10, fieldPos.y - 10));
    }
  }

  auto formRefreshRequired() -> bool { return true; }

  auto computeFieldPos(Pos fromPos) -> void { fieldPos = fromPos; }

  auto paint(Page::Format &fmt) -> void {
    char val[8];
    Glyph *glyph   = font.getGlyph('M', FORM_FONT_SIZE);
    uint8_t offset = -glyph->yoff;

    double v = *formEntry.u.floatVal.value;
    if ((v <= formEntry.u.floatVal.min) && (v >= formEntry.u.floatVal.max)) {
      v = 1.0;
    }
    *formEntry.u.floatVal.value = v;

    float_to_str(v, val, 6, 3);
    page.putStrAt(formEntry.caption, Pos(captionPos.x, captionPos.y + offset), fmt);
    page.putStrAt(val, Pos(fieldPos.x, fieldPos.y + offset), fmt);
  }

  auto event(const EventMgr::Event &event) -> bool {
    if (!eventControl) {
      keypadViewer->show(*formEntry.u.floatVal.value, formEntry.caption);
      eventControl = true;
    } else {
      if (!keypadViewer->event(event)) {
        double v = keypadViewer->getFloatValue();
        if ((v <= formEntry.u.floatVal.min) && (v >= formEntry.u.floatVal.max)) {
          v = 1.0;
        }
        *formEntry.u.floatVal.value = v;
        eventControl                = false;

        return false; // release events control
      }
    }
    return true; // keep events control
  }

  auto updateHighlight() -> void {}

  auto saveValue() -> void {}

  auto computeFieldDim() -> void { font.getASCIISize("XXXXXXX", &fieldDim, FORM_FONT_SIZE); }
};

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL || TOUCH_MENU
class FormDone : public FormField {

public:
  using FormField::FormField;

  FormDone()  = default;
  ~FormDone() = default;

  static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
    return makeUniqueHimem<FormDone>(formEntry, font, page);
  }

  // bool event(const EventMgr::Event &event);

  const Dim getFieldDim() { return Dim(fieldDim.width, fieldDim.height + 10); }
  auto saveValue() -> void {}
  auto updateHighlight() -> void {
    page.putRounded(Dim(fieldDim.width + 16, fieldDim.height + 16),
                    Pos(fieldPos.x - 8, fieldPos.y - 8));
    page.putRounded(Dim(fieldDim.width + 18, fieldDim.height + 18),
                    Pos(fieldPos.x - 9, fieldPos.y - 9));
    page.putRounded(Dim(fieldDim.width + 20, fieldDim.height + 20),
                    Pos(fieldPos.x - 10, fieldPos.y - 10));
  }

  auto computeFieldDim() -> void {
    font.getASCIISize(formEntry.caption, &fieldDim, FORM_FONT_SIZE);
  }

  auto computeFieldPos(Pos fromPos) -> void {
    fieldPos.x = (Screen::getWidth() / 2) - (fieldDim.width / 2);
    fieldPos.y = fromPos.y + 10;
  }

  auto paint(Page::Format &fmt) -> void {
    Glyph *glyph   = font.getGlyph('M', FORM_FONT_SIZE);
    uint8_t offset = -glyph->yoff;

    page.putStrAt(formEntry.caption, Pos(fieldPos.x, fieldPos.y + offset), fmt);
  }
};
#endif

class FieldFactory {
public:
  static auto create(FormEntry &entry, Font &font, Page &page) -> FormFieldPtr {
    switch (entry.entryType) {
    case FormEntryType::HORIZONTAL:
      return HFormChoiceField::Make(entry, font, page);
    case FormEntryType::VERTICAL:
      if ((Screen::getWidth() > Screen::getHeight()) && (entry.u.ch.choiceCount <= 4)) {
        return HFormChoiceField::Make(entry, font, page);
      } else {
        return VFormChoiceField::Make(entry, font, page);
      }
    case FormEntryType::UINT16:
      return FormUInt16::Make(entry, font, page);
    case FormEntryType::FLOAT:
      return FormFloat::Make(entry, font, page);
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL || TOUCH_MENU
    case FormEntryType::DONE:
      return FormDone::Make(entry, font, page);
#endif
    }
    return nullptr;
  }
};

using FormViewerPtr = HimemUniquePtr<class FormViewer>;
class FormViewer {
private:
  FormViewer() = default;

  PagePtr page{Page::Make(appFonts)};

public:
  using FormEntries = FormEntry *;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> HimemUniquePtr<T>;

  static inline auto Make() { return makeUniqueHimem<FormViewer>(); }

  ~FormViewer() {
    // LOG_I("FormViewer destructor called");
    fields.clear();
  }

private:
  static constexpr char const *TAG = "FormViewer";

  static constexpr uint8_t TOP_YPOS    = 100;
  static constexpr uint8_t BOTTOM_YPOS = 50;

  FormEntries formEntries;
  uint8_t size;
  const char *title;
  const char *bottomMsg;

  int16_t allFieldsWidth, allCaptionsWidth;
  // int16_t lastChoicesWidth;
  int8_t lineHeight;
  bool highlightingField;
  bool selectingField;
  bool completed;

  using Fields = std::list<FormFieldPtr>;

  Fields fields;
  Fields::iterator currentField;

  Pos bottomMsgPos;

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  auto findField(uint16_t x, uint16_t y) -> Fields::iterator {
    for (Fields::iterator it = fields.begin(); it != fields.end(); ++it) {
      if ((*it)->isPointed(x, y)) return it;
    }

    return fields.end();
  }

#endif

public:
  auto setCompleted(bool value) -> void { completed = value; }

  void show(const char *titleArg, FormEntries formEntriesArg, int8_t sizeArg,
            const char *bottomMsgArg, bool refresh = false);
  auto event(const EventMgr::Event &event) -> bool;
};
