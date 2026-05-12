// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"

#include "himem.hpp"

#include "controllers/event_mgr.hpp"
#include "fonts.hpp"
#include "viewers/page.hpp"

using KeypadViewerPtr = HimemUniquePtr<class KeypadViewer>;

class KeypadViewer {
private:
  KeypadViewer() = default;
  PagePtr page{Page::Make(appFonts)};

public:
  auto getIntValue() -> uint16_t { return clientUIntValue; }
  auto getFloatValue() -> double { return clientFloatValue; }

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> HimemUniquePtr<T>;

  static inline auto Make() { return makeUniqueHimem<KeypadViewer>(); }
  ~KeypadViewer() = default;

private:
  static constexpr char const *TAG = "KeypadViewer";

  enum class Layout {
    INT, FLOAT,
  };

  Layout layout{Layout::INT};

  static const uint8_t MAX_INT_SIZE            = 4;
  static const uint8_t MAX_FLOAT_FRACTION_SIZE = 3;
  static const uint8_t MAX_FLOAT_INTEGER_SIZE  = 1;
  static const uint8_t MAX_FLOAT_SIZE = MAX_FLOAT_INTEGER_SIZE + 1 + MAX_FLOAT_FRACTION_SIZE;
  static const uint8_t KEY_COUNT      = 14;
  static const uint8_t FONT_SIZE      = 9;

  static const uint8_t NUMBER_STR_SIZE = MAX_FLOAT_SIZE + 1;

  struct KeyLocation {
    Pos pos;
    int8_t value; // 0-9 => value, -1 => cancel, -2 => OK, -3 => BackSpace, -4 => Clear
  };

  static const uint8_t KEY_ADDED_WIDTH  = 50;
  static const uint8_t KEY_ADDED_HEIGHT = 30;

  #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
    KeyLocation *matrix[5][3];
    KeyLocation *currentKey;
    KeyLocation *previousKey;
    uint8_t line, col;
  #endif

  KeyLocation keyLocs[KEY_COUNT];
  Dim keypadDim;
  Pos keypadPos;
  Dim keyDim, keyDim2;
  char numberStr[NUMBER_STR_SIZE];
  uint8_t numberStrCount;
  Pos fieldPos;             // Where to put the number on screen
  uint16_t clientUIntValue; // Computed value
  double clientFloatValue;  // Computed value
  Page::Format fmt;
  Glyph *glyph;

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    auto getKeyVal(uint16_t x, uint16_t y) -> int8_t {
      for (int i = 0; i < (KEY_COUNT - 1); i++) {
        if ((x >= keyLocs[i].pos.x) && (x <= keyLocs[i].pos.x + keyDim.width) &&
            (y >= keyLocs[i].pos.y) && (y <= keyLocs[i].pos.y + keyDim.height)) {
          return keyLocs[i].value;
        }
      }
      if ((x >= keyLocs[KEY_COUNT - 1].pos.x) &&
          (x <= keyLocs[KEY_COUNT - 1].pos.x + keyDim2.width) &&
          (y >= keyLocs[KEY_COUNT - 1].pos.y) &&
          (y <= keyLocs[KEY_COUNT - 1].pos.y + keyDim2.height)) {
        return keyLocs[KEY_COUNT - 1].value;
      }
      return 99; // Not found
    }
  #endif

  auto updateValue() -> void {
    page->clearRegion(Dim(keypadDim.width - 6, keyDim.height - 6),
                      Pos(fieldPos.x + 3, fieldPos.y + 3));
    page->putStrAt(numberStr,
                   Pos(fieldPos.x + (keypadDim.width >> 1),
                       fieldPos.y + (glyph->dim.height >> 1) + (keyDim.height >> 1)),
                   fmt);
  }

  auto addDigit(uint8_t d) -> void {
    switch (layout) {
    case Layout::INT:
      if ((numberStrCount == 1) && (numberStr[0] == '0')) {
        numberStr[0] = '0' + d;
      } else if (numberStrCount < MAX_INT_SIZE) {
        numberStr[numberStrCount++] = '0' + d;
        numberStr[numberStrCount]   = 0;
      }
      break;
    case Layout::FLOAT:
      if ((numberStrCount == 1) && (numberStr[0] == '0')) {
        numberStr[0]   = '0' + d;
        numberStr[1]   = '.';
        numberStr[2]   = 0;
        numberStrCount = 2;
      } else if (numberStrCount < MAX_FLOAT_SIZE) {
        numberStr[numberStrCount++] = '0' + d;
        numberStr[numberStrCount]   = 0;
      }
      break;
    }
  }

  auto clearDigits() -> void {
    numberStr[0]   = '0';
    numberStr[1]   = 0;
    numberStrCount = 1;
  }

  auto removeDigit() -> void {
    switch (layout) {
    case Layout::INT:
      if (numberStrCount == 1) {
        numberStr[0] = '0';
      } else {
        numberStr[--numberStrCount] = 0;
      }
      break;
    case Layout::FLOAT:
      if (numberStrCount <= (MAX_FLOAT_INTEGER_SIZE + 1)) {
        numberStr[0]   = '0';
        numberStr[1]   = 0;
        numberStrCount = 1;
      } else {
        numberStr[--numberStrCount] = 0;
      }
      break;
    }
  }

  #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
    auto updateHighlight() -> void {
      if (previousKey != currentKey) {
        if (previousKey != nullptr) {
          if (previousKey->value == -1) { // Cancel key
            page->clearHighlight(Dim(keyDim2.width - 2, keyDim2.height - 2),
                                 Pos(previousKey->pos.x + 1, previousKey->pos.y + 1));
            page->clearHighlight(Dim(keyDim2.width - 4, keyDim2.height - 4),
                                 Pos(previousKey->pos.x + 2, previousKey->pos.y + 2));
            page->clearHighlight(Dim(keyDim2.width - 6, keyDim2.height - 6),
                                 Pos(previousKey->pos.x + 3, previousKey->pos.y + 3));
          } else {
            page->clearHighlight(Dim(keyDim.width - 2, keyDim.height - 2),
                                 Pos(previousKey->pos.x + 1, previousKey->pos.y + 1));
            page->clearHighlight(Dim(keyDim.width - 4, keyDim.height - 4),
                                 Pos(previousKey->pos.x + 2, previousKey->pos.y + 2));
            page->clearHighlight(Dim(keyDim.width - 6, keyDim.height - 6),
                                 Pos(previousKey->pos.x + 3, previousKey->pos.y + 3));
          }
        }
        if (currentKey->value == -1) { // Cancel key
          page->putHighlight(Dim(keyDim2.width - 2, keyDim2.height - 2),
                             Pos(currentKey->pos.x + 1, currentKey->pos.y + 1));
          page->putHighlight(Dim(keyDim2.width - 4, keyDim2.height - 4),
                             Pos(currentKey->pos.x + 2, currentKey->pos.y + 2));
          page->putHighlight(Dim(keyDim2.width - 6, keyDim2.height - 6),
                             Pos(currentKey->pos.x + 3, currentKey->pos.y + 3));
        } else {
          page->putHighlight(Dim(keyDim.width - 2, keyDim.height - 2),
                             Pos(currentKey->pos.x + 1, currentKey->pos.y + 1));
          page->putHighlight(Dim(keyDim.width - 4, keyDim.height - 4),
                             Pos(currentKey->pos.x + 2, currentKey->pos.y + 2));
          page->putHighlight(Dim(keyDim.width - 6, keyDim.height - 6),
                             Pos(currentKey->pos.x + 3, currentKey->pos.y + 3));
        }
      }
    }
  #endif

public:
  auto display(const char *caption) -> void {
    FontPtr &font = appFonts.getFont(1);
    glyph         = font->getGlyph('0', FONT_SIZE);

    keyDim  = Dim(glyph->dim.width + KEY_ADDED_WIDTH, glyph->dim.height + KEY_ADDED_HEIGHT);
    keyDim2 = Dim((keyDim.width << 1) + 2, keyDim.height);

    LOG_D("Key Dim: [%d, %d], Dim2: [%d, %d]", keyDim.width, keyDim.height, keyDim2.width,
          keyDim2.height);

    keypadDim = Dim(((keyDim.width + 2) * 3) - 2, (keyDim.height + 2) * 7);

    keypadPos = Pos((Screen::getWidth() >> 1) - (keypadDim.width >> 1),
                    (Screen::getHeight() >> 1) - (keypadDim.height >> 1));

    fieldPos = Pos(keypadPos.x, keypadPos.y + keyDim.height + 2);

    LOG_D("Keypad Dim: [%d, %d], Pos: [%d, %d]", keypadDim.width, keypadDim.height, keypadPos.x,
          keypadPos.y);

    // Show keypad on screen

    fmt = {
        .fontSize     = FONT_SIZE,
        .marginLeft   = 5,
        .marginRight  = 5,
        .screenLeft   = 20,
        .screenRight  = 20,
        .screenTop    = 0,
        .screenBottom = 0,
        .align        = CSS::Align::CENTER,
    };

    #if 1 // INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      page->start(fmt);
    #endif

    // The large rectangle into which the keypad will be drawn

    page->clearRegion(Dim(keypadDim.width + 26, keypadDim.height + 26),
                      Pos(keypadPos.x - 13, keypadPos.y - 13));
    page->putHighlight(Dim(keypadDim.width + 20, keypadDim.height + 20),
                       Pos(keypadPos.x - 10, keypadPos.y - 10));

    constexpr int8_t values[14] = {7, 8, 9, 4, 5, 6, 1, 2, 3, -4, 0, -3, -2, -1};
    constexpr char labels[12]   = "789456123 0";

    page->putStrAt(
        caption,
        Pos(Screen::getWidth() >> 1, keypadPos.y + (glyph->dim.height >> 1) + (keyDim.height >> 1)),
        fmt);

    page->putHighlight(Dim(keypadDim.width, keyDim.height), Pos(fieldPos.x, fieldPos.y));
    page->putHighlight(Dim(keypadDim.width - 2, keyDim.height - 2),
                       Pos(fieldPos.x + 1, fieldPos.y + 1));
    page->putHighlight(Dim(keypadDim.width - 4, keyDim.height - 4),
                       Pos(fieldPos.x + 2, fieldPos.y + 2));

    updateValue();

    Pos the_pos = Pos(keypadPos.x, keypadPos.y + (keyDim.height << 1) + 4);

    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      line = col = 0;
    #endif

    for (int i = 0; i < 14; i++) {
      keyLocs[i] = {.pos = the_pos, .value = values[i]};

      #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
        matrix[line][col] = &keyLocs[i];
        if (++col >= 3) {
          col = 0;
          line++;
        }
      #endif

      LOG_D("Key %d pos: [%d, %d]", i, the_pos.x, the_pos.y);

      switch (values[i]) {
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
      case 9:
        page->putHighlight(keyDim, the_pos);
        page->putCharAt(labels[i],
                        Pos(the_pos.x + (keyDim.width >> 1) - (glyph->dim.width >> 1),
                            the_pos.y + (glyph->dim.height >> 1) + (keyDim.height >> 1)),
                        fmt);
        break;
      case -1: // Cancel
        page->putHighlight(keyDim2, the_pos);
        #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
          page->putHighlight(Dim(keyDim2.width - 2, keyDim2.height - 2),
                             Pos(the_pos.x + 1, the_pos.y + 1));
        #endif
        page->putStrAt("CANCEL",
                       Pos(the_pos.x + (keyDim2.width >> 1),
                           the_pos.y + (glyph->dim.height >> 1) + (keyDim.height >> 1)),
                       fmt);
        break;
      case -2: // OK
        page->putHighlight(keyDim, the_pos);
        #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
          page->putHighlight(Dim(keyDim.width - 2, keyDim.height - 2),
                             Pos(the_pos.x + 1, the_pos.y + 1));
        #endif
        page->putStrAt("OK",
                       Pos(the_pos.x + (keyDim.width >> 1),
                           the_pos.y + (glyph->dim.height >> 1) + (keyDim.height >> 1)),
                       fmt);
        break;
      case -3: // Delete
        page->putHighlight(keyDim, the_pos);
        page->putStrAt("DEL",
                       Pos(the_pos.x + (keyDim.width >> 1),
                           the_pos.y + (glyph->dim.height >> 1) + (keyDim.height >> 1)),
                       fmt);
        break;
      case -4: // Clear
        page->putHighlight(keyDim, the_pos);
        page->putStrAt("CLR",
                       Pos(the_pos.x + (keyDim.width >> 1),
                           the_pos.y + (glyph->dim.height >> 1) + (keyDim.height >> 1)),
                       fmt);
        break;
      }
      if (((i + 1) % 3) == 0) {
        the_pos.x = keypadPos.x;
        the_pos.y += keyDim.height + 2;
      } else {
        the_pos.x += keyDim.width + 2;
      }
    }

    #if !(INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL)
      matrix[4][2] = matrix[4][1];
      line = col  = 1;
      previousKey = nullptr;
      currentKey  = matrix[1][1];
      updateHighlight();
      previousKey = currentKey;
    #endif

    updateValue();

    #if 1 // INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      page->paint(false);
    #endif
  }

  auto show(uint16_t value, const char *caption) -> void {

    layout = Layout::INT;

    clientUIntValue = value;
    if (clientUIntValue > 9999) clientUIntValue = 9999;

    int_to_str(clientUIntValue, numberStr, 5);

    numberStrCount = strlen(numberStr);

    // LOG_I("Digits: %s", numberStr);

    display(caption);
  }

  // BE AWARE THAT this is used only for the battery trim field, which is a float between 0.0
  // and 2.0, with 3 digits after the dot, so we can use a fixed size string of 6 chars (1 for the
  // integer part, 1 for the dot, and 3 for the fraction part, plus the null terminator)
  auto show(double value, const char *caption) -> void {

    layout = Layout::FLOAT;

    clientFloatValue = value;
    if ((clientFloatValue >= 2.0) || (clientFloatValue <= 0.0)) clientFloatValue = 1.0;

    float_to_str(clientFloatValue, numberStr, 6, 3);

    numberStrCount = strlen(numberStr);

    // LOG_I("Digits: %s", numberStr);

    display(caption);
  }

  /**
   * @brief Event processing
   *
   * @param event Event coming from the user interaction
   * @return true The Keypad must keep control of events
   * @return false Events processing is complete
   */
  auto event(const EventMgr::Event &event) -> bool {

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      if (event.kind == EventMgr::EventKind::TAP) {
        int8_t value = getKeyVal(event.x, event.y);
        if (value != 99) {
          if (value >= 0)
            addDigit(value);
          else if (value == -1)
            return false;
          else if (value == -2) {
            if (layout == Layout::INT) {
              clientUIntValue = atoi(numberStr);
            } else {
              clientFloatValue = atof(numberStr);
            }
            return false;
          } else if (value == -3)
            removeDigit();
          else if (value == -4)
            clearDigits();
        }
      }
      page->start(fmt);
      updateValue();
      page->paint(false);

      return true;
    #else
      switch (event.kind) {
        #if EXTENDED_CASE
        case EventMgr::EventKind::PREV:
        #else
        case EventMgr::EventKind::DBL_PREV:
        #endif
        col        = (col == 0) ? 2 : col - 1;
        currentKey = matrix[line][col];
        break;

        #if EXTENDED_CASE
        case EventMgr::EventKind::NEXT:
        #else
        case EventMgr::EventKind::DBL_NEXT:
        #endif
        col        = (col == 2) ? 0 : col + 1;
        currentKey = matrix[line][col];
        break;

        #if EXTENDED_CASE
        case EventMgr::EventKind::DBL_PREV:
        #else
        case EventMgr::EventKind::PREV:
        #endif
        line       = (line == 0) ? 4 : line - 1;
        currentKey = matrix[line][col];
        break;

        #if EXTENDED_CASE
        case EventMgr::EventKind::DBL_NEXT:
        #else
        case EventMgr::EventKind::NEXT:
        #endif
        line       = (line == 4) ? 0 : line + 1;
        currentKey = matrix[line][col];
        break;

      case EventMgr::EventKind::SELECT: {
        int8_t value = currentKey->value;
        if (value != 99) {
          if (value >= 0)
            addDigit(value);
          else if (value == -1)
            return false;
          else if (value == -2) {
            if (layout == Layout::INT) {
              clientUIntValue = atoi(numberStr);
            } else {
              clientFloatValue = atof(numberStr);
            }
            return false;
          } else if (value == -3)
            removeDigit();
          else if (value == -4)
            clearDigits();
        }
      } break;

      case EventMgr::EventKind::DBL_SELECT:
        return false;
        break;

      default:
        break;
      }

      page->start(fmt);
      updateValue();
      updateHighlight();
      previousKey = currentKey;
      page->paint(false);

      return true;
    #endif
  }
};

#if 0

Keypads:

   +-----------------+
   |     Caption     |
   +-----------------+
   |    Int Value    |
   +-----+-----+-----+
   |  7  |  8  |  9  |
   +-----+-----+-----+
   |  4  |  5  |  6  |
   +-----+-----+-----+
   |  1  |  2  |  3  |
   +-----+-----+-----+
   | CLR |  0  | DEL |
   +-----+-----+-----+
   | OK  |  CANCEL   |
   +-----+-----------+

   +-----------------+
   |     Caption     |
   +-----------------+
   |   Float Value   |
   +-----+-----+-----+
   |  7  |  8  |  9  |
   +-----+-----+-----+
   |  4  |  5  |  6  |
   +-----+-----+-----+
   |  1  |  2  |  3  |
   +-----+-----+-----+
   |  .  |  0  | DEL |
   +-----+-----+-----+
   | OK  |  CANCEL   |
   +-----+-----------+

#endif