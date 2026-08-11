// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"
#include "himem_pool.hpp"

#include "platform.hpp"

#include "config.hpp"
#include "controllers/event_mgr.hpp"
#include "fonts.hpp"
#include "fonts_db.hpp"
#include "viewers/keyboard_viewer.hpp"
#include "viewers/keypad_viewer.hpp"
#include "viewers/page.hpp"
#include "viewers/screen_bottom.hpp"

#include <list>
#include <string>
#include <variant>

enum class FormEntryType {
  CHOICE,
  UINT16,
  FLOAT,
  STRING,
  #if TOUCH_ENABLE || TOUCH_MENU
    DONE
  #endif
};

constexpr uint8_t FORM_FONT_SIZE = 9;

struct FormChoice {
  const char *caption;
  int8_t value;
  ~FormChoice() = default;
};

struct ChoiceEntry {
  int8_t *value;
  int8_t choiceCount;
  const FormChoice *choices;
  ~ChoiceEntry() = default;
};

struct IntEntry {
  uint16_t *value;
  uint16_t min;
  uint16_t max;
  ~IntEntry() = default;
};

struct FloatEntry {
  double *value;
  double min;
  double max;
  ~FloatEntry() = default;
};

struct StringEntry {
  HimemString *value;
  uint16_t maxLen;
  bool hide;

  ~StringEntry() = default;
};

class FormEntry {
  public:
    FormEntryType entryType;
    const char *caption;
    std::variant<ChoiceEntry, IntEntry, FloatEntry, StringEntry> u;
    ~FormEntry() {};
};

using FormFieldPtr = HimemUniquePtr<class FormField>;
class FormField {
  public:
    enum class State : uint8_t { NORMAL, HIGHLIGHTED, SELECTED };

  private:
    static constexpr char const *TAG = "FormField";

  protected:
    FormEntry &formEntry;
    Font &font;
    Page &page;
    bool eventControl; // True if the field is currently in control of events (e.g., a keypad is open for input)
    Dim fieldDim, captionDim;
    Pos fieldPos, captionPos;
    State state;
    uint16_t rightXPos;
    uint16_t baseOffset;
    uint8_t savedLineHeight;

  public:

    FormField(FormEntry &formEntry, Font &font, Page &page)
      : formEntry(formEntry), font(font), page(page), eventControl(false), state(State::NORMAL) {};
    virtual ~FormField() = default; // { LOG_I("FormField destructor called"); }

    virtual auto getFieldDim() -> const Dim { return fieldDim; }
    [[nodiscard]] inline auto getCaptionDim() -> const Dim & { return captionDim; }

    [[nodiscard]] inline auto getFieldPos() -> const Pos & { return fieldPos; }
    [[nodiscard]] inline auto getCaptionPos() -> const Pos & { return captionPos; }

    [[nodiscard]] inline auto getType() -> FormEntryType { return formEntry.entryType; }

    auto computeCaptionDim(uint8_t lineHeight) -> void {
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wsequence-point"

      captionDim = (formEntry.caption != nullptr) ?
                   font.getASCIISize(formEntry.caption, FORM_FONT_SIZE) :
                   Dim(0, 0);
      captionDim.height = lineHeight;
      savedLineHeight = lineHeight;
      #pragma GCC diagnostic pop
    }

    virtual auto formRefreshRequired() -> bool { return false; }
    virtual auto paintCaption(Page::Format &fmt) -> void = 0;
    virtual auto computeDim(uint8_t lineHeight) -> void  = 0;
    virtual auto computePos(Pos fromPos) -> void         = 0;
    virtual auto saveValue() -> void                     = 0;

    virtual auto event(EventMgr::Event &event, 
                       Page::Format &fmt) -> bool { return false; }

    [[nodiscard]] inline auto inEventControl() -> bool { return eventControl; }

    auto computeCaptionPos(Pos fromPos, uint16_t offset) -> void {
      baseOffset = offset;
      captionPos = Pos(fromPos.x, fromPos.y);
    }

    virtual auto updateState(State state, Page::Format &fmt) -> void = 0;
    virtual auto update(Page::Format &fmt) -> void = 0;
    
    auto getState() -> State { return state; }

    #if TOUCH_ENABLE
      [[nodiscard]] inline auto isPointed(uint16_t x, uint16_t y) -> bool {
        return (x >= (fieldPos.x - 10)) && (y >= (fieldPos.y - 7)) &&
               (x <= (fieldPos.x + fieldDim.width + 10)) && (y <= (fieldPos.y + baseOffset + 7));
      }

    #endif
};

class FormChoiceField : public FormField {
  private:
    constexpr static char const *TAG = "FormChoiceField";
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
    static constexpr FormChoice dirViewChoices[2] = { { "LINEAR", 0 }, { "MATRIX", 1 } };

    static constexpr FormChoice coverSizeChoices[3] = { { "SMALL", 0 }, { "MEDIUM", 1 }, { "LARGE", 2 } };

    static constexpr FormChoice lineHeightChoices[3] = { { "TIGHT", 0 }, { "MEDIUM", 1 }, { "LARGE", 2 } };

    static constexpr FormChoice okCancelChoices[2] = { { "OK", 1 }, { "CANCEL", 0 } };

    static constexpr FormChoice yesNoChoices[2] = { { "YES", 1 }, { "NO", 0 } };

    static constexpr FormChoice resolutionChoices[2] = { { "1Bit", 0 }, { "3Bits", 1 } };

    static constexpr FormChoice timeoutChoices[3] = { { "5", 5 }, { "15", 15 }, { "30", 30 } };

    static constexpr FormChoice columnCountChoices[4] = { { "1", 1 }, { "2", 2 }, { "3", 3 }, { "4", 4 } };

    static constexpr FormChoice batteryVisualChoices[4] = {
      { "NONE", 0 }, { "PERCENT", 1 }, { "VOLTAGE", 2 }, { "ICON", 3 } };

    static constexpr FormChoice fontSizeChoices[4] = { { "8", 8 }, { "10", 10 }, { "12", 12 }, { "15", 15 } };

    #if DATE_TIME_RTC
      static constexpr FormChoice rightCornerChoices[3] = { { "NONE", 0 }, { "DATE TIME", 1 }, { "HEAP", 2 } };
    #endif

    #if OLD_INKPLATE
      static constexpr FormChoice orientationChoices[3] = { 
        { "LEFT", 0 }, { "RIGHT", 1 }, { "BOTTOM", 2 } };
    #else
      static constexpr FormChoice orientationChoices[4] = {
        { "LEFT", 3 }, { "RIGHT", 2 }, { "TOP", 1 }, { "BOTTOM", 0 } };
    #endif

    static FormChoice fontChoices[8];
    static uint8_t fontChoicesCount;

    static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
      return makeUniqueHimem<FormChoiceField>(formEntry, font, page);
    }

    auto computeDim(uint8_t lineHeight) -> void {
      fieldDim = { 0, 0 };
      for (int8_t i = 0; i < std::get<ChoiceEntry>(formEntry.u).choiceCount; ++i) {
        Item *item = itemPool.newElement();
        items.push_back(item);
        item->dim = font.getASCIISize(std::get<ChoiceEntry>(formEntry.u).choices[i].caption, FORM_FONT_SIZE);
        item->dim.height = savedLineHeight;
        item->idx = i;
      }

      int i = 0;
      for (Items::iterator it = items.begin(); it != items.end(); ++it) {
        if (std::get<ChoiceEntry>(formEntry.u).choices[i].value == *std::get<ChoiceEntry>(formEntry.u).value) {
          currentItem = it;
          break;
        }
        ++i;
      }
      if (currentItem == items.end()) { currentItem = items.begin(); }

      oldItem = items.end();

      // uint8_t lineHeight = font.getLineHeight(FORM_FONT_SIZE);
      // uint8_t lastHeight = 0;
      for (auto *item : items) {
        if (fieldDim.width < item->dim.width) { fieldDim.width = item->dim.width; }
        if (fieldDim.height < item->dim.height) { fieldDim.height = item->dim.height; }
        LOG_D("Item dimension: [{}, {}]", item->dim.width, item->dim.height);
      }
    }

    static auto adjustFontChoices() -> void {
      FontsDB *fontsDB{ nullptr };

      config.get(Config::Ident::FONTS_DB, &fontsDB);
      auto fontCount = fontsDB ? fontsDB->getStandardFontCount() : 0;
      if (fontCount > 8) { fontCount = 8; }
      for (uint8_t i = 0; i < fontCount; ++i) {
        fontChoices[i].caption = fontsDB->getStandardFontName(i).c_str();
      }
      fontChoicesCount = fontCount;
    }

    auto computePos(Pos fromPos) -> void {
      rightXPos = fromPos.x;
      fieldPos   = { uint16_t(rightXPos - fieldDim.width), fromPos.y };

      // uint8_t lineHeight = font.getLineHeight(FORM_FONT_SIZE);
      for (auto *item : items) {
        item->pos = { uint16_t(rightXPos - item->dim.width), fromPos.y };
        LOG_D("Item position  [{}, {}]", item->pos.x, item->pos.y);
      }
    }

    auto paintCaption(Page::Format &fmt) -> void {
      page.putStrAt(formEntry.caption, Pos(captionPos.x, captionPos.y + baseOffset), fmt);
    }

    #if TOUCH_ENABLE
      auto event(EventMgr::Event &event, Page::Format &fmt) -> bool {
        if (state != State::SELECTED) { return false; }
        oldItem = currentItem;
        ++currentItem;
        if (currentItem == items.end()) { currentItem = items.begin(); }
        update(fmt);    
        return true;    
      }
    #else
      auto event(EventMgr::Event &event, Page::Format &fmt) -> bool override{
        if (state != State::SELECTED) { return false; }
        oldItem = currentItem;
        switch (event.kind) {
        case EventMgr::EventKind::PREV:
          if (currentItem == items.begin()) { currentItem = items.end(); }
          --currentItem;
          update(fmt);
          break;
        case EventMgr::EventKind::NEXT:
          ++currentItem;
          if (currentItem == items.end()) { currentItem = items.begin(); }
          update(fmt);
          break;
        default:
          return false;
        }
        return true;
      }
    #endif

    auto updateState(State newState, Page::Format &fmt) -> void {
      state = newState;
      int8_t thickness = 1;
      switch (state) {
      case State::NORMAL:
        break;
      case State::HIGHLIGHTED:
        thickness = -3;
        break;
      case State::SELECTED:
        thickness = -1;
        break;
      }

      page.clearRounded({uint16_t(fieldDim.width + 20), uint16_t(fieldDim.height + 14) },
                        {uint16_t(rightXPos - fieldDim.width - 10), uint16_t(fieldPos.y - 7) },
                        -1, 20);

      auto fieldDim = font.getASCIISize(std::get<ChoiceEntry>(formEntry.u).choices[(*currentItem)->idx].caption, 
                                        FORM_FONT_SIZE);
      fieldDim.height = savedLineHeight;

      page.putRounded({uint16_t(fieldDim.width + 20), uint16_t(fieldDim.height + 14) },
                      {uint16_t(rightXPos - fieldDim.width - 10), uint16_t(fieldPos.y - 7) },
                      thickness, 20);

      if (state != State::NORMAL && thickness != -3) { fmt.fontStyle = FaceStyle::INVERSE; } 
      page.putStrAt(std::get<ChoiceEntry>(formEntry.u).choices[(*currentItem)->idx].caption,
                    Pos(rightXPos - fieldDim.width, fieldPos.y + baseOffset), fmt);
      if (state != State::NORMAL && thickness != -3) { fmt.fontStyle = FaceStyle::NORMAL; } 

      fieldPos = Pos(rightXPos - fieldDim.width, fieldPos.y);
    }

    auto update(Page::Format &fmt) -> void {
      if (oldItem != currentItem) {
        if (oldItem != items.end()) {
          page.clearRounded({uint16_t((*oldItem)->dim.width + 20), uint16_t((*oldItem)->dim.height + 14) },
                            {uint16_t((*oldItem)->pos.x - 10), uint16_t((*oldItem)->pos.y - 7) },
                             -1, 20);
        }
        updateState(state, fmt);
      }
      oldItem = currentItem;
    }

    auto saveValue() -> void {
      *std::get<ChoiceEntry>(formEntry.u).value = std::get<ChoiceEntry>(formEntry.u).choices[(*currentItem)->idx].value;
    }

    using FormField::FormField;

    ~FormChoiceField() {
      for (auto *item : items) {
        itemPool.deleteElement(item);
      }
      items.clear();
    }
};

using FormUInt16Ptr = HimemUniquePtr<class FormUInt16>;

class FormUInt16 : public FormField {
  private:
    KeypadViewerPtr keypadViewer{ KeypadViewer::Make() };

  public:
    using FormField::FormField;

    FormUInt16()  = default;
    ~FormUInt16() = default;

    static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
      return makeUniqueHimem<FormUInt16>(formEntry, font, page);
    }

    auto formRefreshRequired() -> bool { return true; }

    auto computePos(Pos fromPos) -> void { 

      rightXPos = fromPos.x;
      fieldPos = { uint16_t(rightXPos - fieldDim.width), fromPos.y };
    }

    auto paintCaption(Page::Format &fmt) -> void {
      page.putStrAt(formEntry.caption, Pos(captionPos.x, captionPos.y + baseOffset), fmt);
    }

    auto event(EventMgr::Event &event, Page::Format &fmt) -> bool {
      if (!eventControl) {
        keypadViewer->show(*std::get<IntEntry>(formEntry.u).value, formEntry.caption);
        eventControl = true;
      } else {
        if (!keypadViewer->event(event)) {
          uint16_t v = keypadViewer->getIntValue();
          if (v < std::get<IntEntry>(formEntry.u).min) {
            v = std::get<IntEntry>(formEntry.u).min;
          } else if (v > std::get<IntEntry>(formEntry.u).max) {
            v = std::get<IntEntry>(formEntry.u).max;
          }
          *std::get<IntEntry>(formEntry.u).value = v;
          eventControl              = false;

          return false; // release events control
        }
      }
      return true; // keep events control
    }

    auto updateState(State newState, Page::Format &fmt) -> void {

      char     val[8];

      uint16_t v = *std::get<IntEntry>(formEntry.u).value;
      if (v < std::get<IntEntry>(formEntry.u).min) {
        v = std::get<IntEntry>(formEntry.u).min;
      } else if (v > std::get<IntEntry>(formEntry.u).max) {
        v = std::get<IntEntry>(formEntry.u).max;
      }
      *std::get<IntEntry>(formEntry.u).value = v;

      int_to_str(v, val, 8);

      state = newState;
      int8_t thickness = 1;
      switch (state) {
      case State::NORMAL:
        break;
      case State::HIGHLIGHTED:
        thickness = -3;
        break;
      case State::SELECTED:
        thickness = -1;
        break;
      }
      page.clearRounded({uint16_t(fieldDim.width + 20), uint16_t(fieldDim.height + 14) },
                        {uint16_t(rightXPos - fieldDim.width - 10), uint16_t(fieldPos.y - 7) },
                        -1, 20);

      auto fieldDim = font.getASCIISize(val, FORM_FONT_SIZE);
      fieldDim.height = savedLineHeight;

      page.putRounded({uint16_t(fieldDim.width + 20), uint16_t(fieldDim.height + 14) },
                      {uint16_t(rightXPos - fieldDim.width - 10), uint16_t(fieldPos.y - 7) },
                      thickness, 20);

      if (state != State::NORMAL && thickness != -3) { fmt.fontStyle = FaceStyle::INVERSE; } 
      page.putStrAt(val, Pos(rightXPos - fieldDim.width, fieldPos.y + baseOffset), fmt);
      if (state != State::NORMAL && thickness != -3) { fmt.fontStyle = FaceStyle::NORMAL; } 

      fieldPos = Pos(rightXPos - fieldDim.width, fieldPos.y);
    }

    auto update(Page::Format &fmt) -> void {
        updateState(State::SELECTED, fmt);
    }

    auto saveValue() -> void {}

    auto computeDim(uint8_t lineHeight) -> void {
      fieldDim = font.getASCIISize("XXXXX", FORM_FONT_SIZE);
      fieldDim.height = lineHeight;
    }
};

using FormFloatPtr = HimemUniquePtr<class FormFloat>;

class FormFloat : public FormField {
  private:
    static constexpr char const *TAG = "FormFloat";
    KeypadViewerPtr keypadViewer{ KeypadViewer::Make() };

  public:
    using FormField::FormField;

    FormFloat()  = default;
    ~FormFloat() = default;

    static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
      return makeUniqueHimem<FormFloat>(formEntry, font, page);
    }

    auto formRefreshRequired() -> bool { return true; }

    auto computePos(Pos fromPos) -> void { 
      rightXPos = fromPos.x; 
      fieldPos = { uint16_t(rightXPos - fieldDim.width), fromPos.y };
    }

    auto updateState(State newState, Page::Format &fmt) -> void {
      char    val[8];

      double  v = *std::get<FloatEntry>(formEntry.u).value;
      if ((v <= std::get<FloatEntry>(formEntry.u).min) && (v >= std::get<FloatEntry>(formEntry.u).max)) {
        v = 1.0;
      }
      *std::get<FloatEntry>(formEntry.u).value = v;

      float_to_str(v, val, 6, 3);

      state = newState;
      int8_t thickness = 1;

      switch (state) {
      case State::NORMAL:
        break;
      case State::HIGHLIGHTED:
        thickness = -3;
        break;
      case State::SELECTED:
        thickness = -1;
        break;
      }

      page.clearRounded({uint16_t(fieldDim.width + 20), uint16_t(fieldDim.height + 14) },
                        {uint16_t(rightXPos - fieldDim.width - 10), uint16_t(fieldPos.y - 7) },
                        -1, 20);

      fieldDim = font.getASCIISize(val, FORM_FONT_SIZE);
      fieldDim.height = savedLineHeight;

      page.putRounded({uint16_t(fieldDim.width + 20), uint16_t(fieldDim.height + 14) },
                      {uint16_t(rightXPos - fieldDim.width - 10), uint16_t(fieldPos.y - 7) },
                      thickness, 20);

      if (state != State::NORMAL && thickness != -3) { fmt.fontStyle = FaceStyle::INVERSE; } 
      page.putStrAt(val, Pos(rightXPos - fieldDim.width, fieldPos.y + baseOffset), fmt);
      if (state != State::NORMAL && thickness != -3) { fmt.fontStyle = FaceStyle::NORMAL; } 

      fieldPos = Pos(rightXPos - fieldDim.width, fieldPos.y);
    }

    auto update(Page::Format &fmt) -> void {
        updateState(State::SELECTED, fmt);
    }

    auto paintCaption(Page::Format &fmt) -> void {
      page.putStrAt(formEntry.caption, Pos(captionPos.x, captionPos.y + baseOffset), fmt);
    }

    auto event(EventMgr::Event &event, Page::Format &fmt) -> bool {
      if (!eventControl) {
        keypadViewer->show(*std::get<FloatEntry>(formEntry.u).value, formEntry.caption);
        eventControl = true;
      } else {
        if (!keypadViewer->event(event)) {
          double v = keypadViewer->getFloatValue();
          if ((v <= std::get<FloatEntry>(formEntry.u).min) && (v >= std::get<FloatEntry>(formEntry.u).max)) {
            v = 1.0;
          }
          *std::get<FloatEntry>(formEntry.u).value = v;
          eventControl                = false;

          return false; // release events control
        }
      }
      return true; // keep events control
    }

    auto saveValue() -> void {}

    auto computeDim(uint8_t lineHeight) -> void { 
      fieldDim = font.getASCIISize("XXXXXXX", FORM_FONT_SIZE);
      fieldDim.height = lineHeight;
    }
};

using FormFloatPtr = HimemUniquePtr<class FormFloat>;

class FormString : public FormField {
  private:
    static constexpr char const *TAG = "FormString";
    KeyboardViewerPtr keyboardViewer{ KeyboardViewer::Make() };
    std::string currentValue;

  public:
    using FormField::FormField;

    FormString()  = default;
    ~FormString() = default;

    static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
      return makeUniqueHimem<FormString>(formEntry, font, page);
    }

    auto formRefreshRequired() -> bool { return true; }

    auto computePos(Pos fromPos) -> void { 
      rightXPos = fromPos.x; 
      fieldPos = { uint16_t(rightXPos - fieldDim.width), fromPos.y };
    }

    auto updateState(State newState, Page::Format &fmt) -> void {
      char    val[33];

      memcpy(val, std::get<StringEntry>(formEntry.u).value->c_str(), 32);
      val[32] = '\0';

      state = newState;
      int8_t thickness = 1;

      switch (state) {
      case State::NORMAL:
        break;
      case State::HIGHLIGHTED:
        thickness = -3;
        break;
      case State::SELECTED:
        thickness = -1;
        break;
      }

      page.clearRounded({uint16_t(fieldDim.width + 20), uint16_t(fieldDim.height + 14) },
                        {uint16_t(rightXPos - fieldDim.width - 10), uint16_t(fieldPos.y - 7) },
                        -1, 20);

      fieldDim = font.getASCIISize(val[0] == 0 ? "empty" : val, FORM_FONT_SIZE);
      fieldDim.height = savedLineHeight;

      page.putRounded({uint16_t(fieldDim.width + 20), uint16_t(fieldDim.height + 14) },
                      {uint16_t(rightXPos - fieldDim.width - 10), uint16_t(fieldPos.y - 7) },
                      thickness, 20);

      if (state != State::NORMAL && thickness != -3) { fmt.fontStyle = FaceStyle::INVERSE; } 
      page.putStrAt(val[0] == 0 ? "empty" : val, 
                    Pos(rightXPos - fieldDim.width, fieldPos.y + baseOffset), 
                    fmt);
      if (state != State::NORMAL && thickness != -3) { fmt.fontStyle = FaceStyle::NORMAL; } 

      fieldPos = Pos(rightXPos - fieldDim.width, fieldPos.y);
    }

    auto update(Page::Format &fmt) -> void {
        updateState(State::SELECTED, fmt);
    }

    auto paintCaption(Page::Format &fmt) -> void {
      page.putStrAt(formEntry.caption, Pos(captionPos.x, captionPos.y + baseOffset), fmt);
    }

    auto event(EventMgr::Event &event, Page::Format &fmt) -> bool {
      if (!eventControl) {
        currentValue.assign(std::get<StringEntry>(formEntry.u).value->c_str());
        keyboardViewer->show(currentValue, std::get<StringEntry>(formEntry.u).maxLen, formEntry.caption, std::get<StringEntry>(formEntry.u).hide);
        eventControl = true;
      } else {
        if (!keyboardViewer->event(event)) {
          eventControl = false;
          std::get<StringEntry>(formEntry.u).value->assign(currentValue);
          return false; // release events control
        }
      }
      return true; // keep events control
    }

    auto saveValue() -> void {
      // std::get<StringEntry>(formEntry.u).value->assign(currentValue);
      // std::get<StringEntry>(formEntry.u).value[std::get<StringEntry>(formEntry.u).maxLen - 1] = '\0'; // Ensure null-termination
    }

    auto computeDim(uint8_t lineHeight) -> void { 
      fieldDim = font.getASCIISize("XXXXXXXXXXXXXXXXXXXX", FORM_FONT_SIZE);
      fieldDim.height = lineHeight;
    }
};

#if TOUCH_ENABLE || TOUCH_MENU
  class FormDone : public FormField {

    public:
      using FormField::FormField;

      FormDone()  = default;
      ~FormDone() = default;

      static inline auto Make(FormEntry &formEntry, Font &font, Page &page) {
        return makeUniqueHimem<FormDone>(formEntry, font, page);
      }

      // bool event(const EventMgr::Event &event);

      const Dim getFieldDim() { return Dim(fieldDim.width, fieldDim.height + 20); }

      auto saveValue() -> void {}

      auto computeDim(uint8_t lineHeight) -> void {
        fieldDim = font.getASCIISize(formEntry.caption, FORM_FONT_SIZE);
        fieldDim.height = savedLineHeight;
      }

      auto computePos(Pos fromPos) -> void {
        fieldPos.x = (Screen::getWidth() / 2) - (fieldDim.width / 2);
        fieldPos.y = fromPos.y + 10;
      }

      auto updateState(State newState, Page::Format &fmt) -> void {
        state = newState;

        page.putRounded({uint16_t(fieldDim.width + 20), uint16_t(fieldDim.height + 20) },
                        {uint16_t(fieldPos.x - 10), uint16_t(fieldPos.y - 10) },
                        -1, 8);

        fmt.fontStyle = FaceStyle::INVERSE;
        page.putStrAt(formEntry.caption, Pos(fieldPos.x, fieldPos.y + baseOffset), fmt);
        fmt.fontStyle = FaceStyle::NORMAL;
      }

      auto update(Page::Format &fmt) -> void {}
    
      auto paintCaption(Page::Format &fmt) -> void {}
  };
#endif

class FieldFactory {
  public:
    static auto create(FormEntry &entry, Font &font, Page &page) -> FormFieldPtr {
      switch (entry.entryType) {
      case FormEntryType::CHOICE:
        return FormChoiceField::Make(entry, font, page);
      case FormEntryType::UINT16:
        return FormUInt16::Make(entry, font, page);
      case FormEntryType::FLOAT:
        return FormFloat::Make(entry, font, page);
      case FormEntryType::STRING:
        return FormString::Make(entry, font, page);
    #if TOUCH_ENABLE || TOUCH_MENU
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

    PagePtr page{ Page::Make(appFonts) };

  public:
    using FormEntries = FormEntry *;

    template <typename T, typename ... Args>
    requires(!std::is_array_v<T>)
    friend auto makeUniqueHimem(Args &&... args)->HimemUniquePtr<T>;

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

    int16_t maxFieldsWidth, maxCaptionsWidth;
    int8_t lineHeight;
    bool completed;

    using Fields = std::list<FormFieldPtr>;

    Fields fields;
    Fields::iterator oldField, currentField;

    Pos bottomMsgPos;

    Page::Format fmt{};
    uint8_t glyphYOffset;

    #if TOUCH_ENABLE
      auto findField(uint16_t x, uint16_t y) -> Fields::iterator {
        for (Fields::iterator it = fields.begin(); it != fields.end(); ++it) {
          if ((*it)->isPointed(x, y)) { return it; }
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
