// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

//    +-----------------+
//    |      Caption    |
//    +-----------------+
//    |     Int Value   |
//    +-----+-----+-----+
//    |  7  |  8  |  9  |
//    +-----+-----+-----+
//    |  4  |  5  |  6  |
//    +-----+-----+-----+
//    |  1  |  2  |  3  |
//    +-----+-----+-----+
//    | CLR |  0  | DEL |
//    +-----+-----+-----+
//    | OK  |  CANCEL   |
//    +-----+-----------+

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
    PagePtr page{ Page::Make(appFonts) };

  public:
    auto getIntValue() -> uint16_t { return clientUIntValue; }
    auto getFloatValue() -> double { return clientFloatValue; }

    template <typename T, typename ... Args>
    requires(!std::is_array_v<T>)
    friend auto makeUniqueHimem(Args &&... args)->HimemUniquePtr<T>;

    static inline auto Make() { return makeUniqueHimem<KeypadViewer>(); }
    ~KeypadViewer() = default;

  private:
    static constexpr char const *TAG = "KeypadViewer";

    enum class Layout {
      INT, FLOAT,
    };

    Layout layout{ Layout::INT };

    static const uint8_t MAX_INT_LENGTH            = 4;
    static const uint8_t MAX_FLOAT_FRACTION_SIZE = 3;
    static const uint8_t MAX_FLOAT_INTEGER_SIZE  = 1;
    static const uint8_t MAX_FLOAT_LENGTH = MAX_FLOAT_INTEGER_SIZE + 1 + MAX_FLOAT_FRACTION_SIZE;
    static const uint8_t KEY_COUNT      = 14;
    static const uint8_t FONT_SIZE      = 9;

    static const uint8_t NUMBER_STR_SIZE = MAX_FLOAT_LENGTH + 1;

    struct KeyLocation {
      Dim dim;
      Pos pos;
      int8_t value; // 0-9 => value, -1 => cancel, -2 => OK, -3 => BackSpace, -4 => Clear
      char label;
    };

    static const uint8_t KEY_ADDED_WIDTH  = 50;
    static const uint8_t KEY_ADDED_HEIGHT = 30;

    #if !TOUCH_ENABLE
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
    uint8_t numberStrLength;
    Pos fieldPos;           // Where to put the number on screen
    uint16_t clientUIntValue; // Computed value
    double clientFloatValue; // Computed value
    Page::Format fmt{};
    Glyph *glyph;

    #if TOUCH_ENABLE
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
      page->clearRegion(Dim(keypadDim.width - 10, keyDim.height - 10),
                        Pos(fieldPos.x + 5, fieldPos.y + 5));
      page->putStrAt(numberStr,
                     Pos(fieldPos.x + (keypadDim.width >> 1),
                         fieldPos.y + (glyph->dim.height >> 1) + (keyDim.height >> 1)),
                     fmt);
    }

    auto addDigit(uint8_t d) -> void {
      switch (layout) {
      case Layout::INT:
        if ((numberStrLength == 1) && (numberStr[0] == '0')) {
          numberStr[0] = '0' + d;
        } else if (numberStrLength < MAX_INT_LENGTH) {
          numberStr[numberStrLength++] = '0' + d;
          numberStr[numberStrLength]   = 0;
        }
        break;
      case Layout::FLOAT:
        if ((numberStrLength == 1) && (numberStr[0] == '0')) {
          numberStr[0]   = '0' + d;
          numberStr[1]   = '.';
          numberStr[2]   = 0;
          numberStrLength = 2;
        } else if (numberStrLength < MAX_FLOAT_LENGTH) {
          numberStr[numberStrLength++] = '0' + d;
          numberStr[numberStrLength]   = 0;
        }
        break;
      }
    }

    auto clearDigits() -> void {
      numberStr[0]   = '0';
      numberStr[1]   = 0;
      numberStrLength = 1;
    }

    auto removeDigit() -> void {
      switch (layout) {
      case Layout::INT:
        if (numberStrLength == 1) {
          numberStr[0] = '0';
        } else {
          numberStr[--numberStrLength] = 0;
        }
        break;
      case Layout::FLOAT:
        if (numberStrLength <= (MAX_FLOAT_INTEGER_SIZE + 1)) {
          numberStr[0]   = '0';
          numberStr[1]   = 0;
          numberStrLength = 1;
        } else {
          numberStr[--numberStrLength] = 0;
        }
        break;
      }
    }

    void drawButton(const KeyLocation &key, int8_t thickness = 1) {

      if (thickness == -1) { fmt.fontStyle = FaceStyle::INVERSE; }
 
      switch (key.value) {
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
        page->putRounded(key.dim, key.pos, thickness);
        page->putCharAt(key.label,
                        Pos(key.pos.x + (key.dim.width >> 1) - (glyph->dim.width >> 1),
                            key.pos.y + (glyph->dim.height >> 1) + (key.dim.height >> 1)),
                        fmt);
        break;
      case -1: // Cancel
        page->  putRounded(key.dim, key.pos, thickness);
        page->putStrAt("CANCEL",
                        Pos(key.pos.x + (key.dim.width >> 1),
                            key.pos.y + (glyph->dim.height >> 1) + (key.dim.height >> 1)),
                        fmt);
        break;
      case -2: // OK
        page->  putRounded(key.dim, key.pos, thickness);
        page->putStrAt("OK",
                        Pos(key.pos.x + (key.dim.width >> 1),
                            key.pos.y + (glyph->dim.height >> 1) + (key.dim.height >> 1)),
                        fmt);
        break;
      case -3: // Delete
        page->putRounded(key.dim, key.pos, thickness);
        page->putStrAt("DEL",
                        Pos(key.pos.x + (key.dim.width >> 1),
                            key.pos.y + (glyph->dim.height >> 1) + (key.dim.height >> 1)),
                        fmt);
        break;
      case -4: // Clear
        page->putRounded(key.dim, key.pos, thickness);
        page->putStrAt("CLR",
                        Pos(key.pos.x + (key.dim.width >> 1),
                            key.pos.y + (glyph->dim.height >> 1) + (key.dim.height >> 1)),
                        fmt);
        break;  
      }

      fmt.fontStyle = FaceStyle::NORMAL;
    }

    #if !TOUCH_ENABLE
      auto updateHighlight() -> void {
        if (previousKey != currentKey) {
          if (previousKey != nullptr) {
            page->clearRounded(Dim(previousKey->dim.width - 2, previousKey->dim.height - 2),
                               Pos(previousKey->pos.x + 1, previousKey->pos.y + 1), -1);
            drawButton(*previousKey, 1);
          }
          page->clearRounded(Dim(currentKey->dim.width - 2, currentKey->dim.height - 2),
                             Pos(currentKey->pos.x + 1, currentKey->pos.y + 1), -1);
          drawButton(*currentKey, -1);
        }
      }
    #endif

  public:
    auto display(const char *caption) -> void {
      FontPtr &font = appFonts.getFont(1);
      glyph         = font->getGlyph('0', FONT_SIZE);

      keyDim  = Dim(glyph->dim.width + KEY_ADDED_WIDTH, glyph->dim.height + KEY_ADDED_HEIGHT);
      keyDim2 = Dim((keyDim.width << 1) + 2, keyDim.height);

      LOG_D("Key Dim: [{}, {}], Dim2: [{}, {}]", keyDim.width, keyDim.height, keyDim2.width,
            keyDim2.height);

      keypadDim = Dim(((keyDim.width + 2) * 3) - 2, (keyDim.height + 2) * 7);

      keypadPos = Pos((Screen::getWidth() >> 1) - (keypadDim.width >> 1),
                      (Screen::getHeight() >> 1) - (keypadDim.height >> 1));

      fieldPos = Pos(keypadPos.x, keypadPos.y + keyDim.height + 2);

      LOG_D("Keypad Dim: [{}, {}], Pos: [{}, {}]", keypadDim.width, keypadDim.height, keypadPos.x,
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
        .align        = HAlign::CENTER,
      };

      page->start(fmt);

      // The large rectangle into which the keypad will be drawn

      page->clearRegion(Dim(keypadDim.width + 26, keypadDim.height + 26),
                        Pos(keypadPos.x - 13, keypadPos.y - 13));
      page->putRounded(Dim(keypadDim.width + 20, keypadDim.height + 20),
                         Pos(keypadPos.x - 10, keypadPos.y - 10), 2, 8);

      constexpr int8_t values[14] = { 7, 8, 9, 4, 5, 6, 1, 2, 3, -4, 0, -3, -2, -1 };
      constexpr char   labels[15]   = "789456123 0   ";

      page->putStrAt(
        caption,
        Pos(Screen::getWidth() >> 1, keypadPos.y + (glyph->dim.height >> 1) + (keyDim.height >> 1)),
        fmt);


      updateValue();

      Pos the_pos = Pos(keypadPos.x, keypadPos.y + (keyDim.height << 1) + 6);

      #if !TOUCH_ENABLE
        line = col = 0;
      #endif

      for (int i = 0; i < 14; i++) {
        keyLocs[i] = { .dim = i == 13 ? keyDim2 : keyDim, .pos = the_pos, .value = values[i], .label = labels[i] };

        #if !TOUCH_ENABLE
          matrix[line][col] = &keyLocs[i];
          if (++col >= 3) {
            col = 0;
            line++;
          }
        #endif

        LOG_D("Key {} pos: [{}, {}]", i, the_pos.x, the_pos.y);

        drawButton(keyLocs[i]);

        if (((i + 1) % 3) == 0) {
          the_pos.x = keypadPos.x;
          the_pos.y += keyDim.height + 2;
        } else {
          the_pos.x += keyDim.width + 2;
        }
      }

      page->putRounded(Dim(keypadDim.width, keyDim.height), Pos(fieldPos.x, fieldPos.y), 3);

      #if !TOUCH_ENABLE
        matrix[4][2] = matrix[4][1];
        line = col  = 1;
        previousKey = nullptr;
        currentKey  = matrix[1][1];
        updateHighlight();
        previousKey = currentKey;
      #endif

      updateValue();

      page->paint(false);
    }

    auto show(uint16_t value, const char *caption) -> void {

      layout          = Layout::INT;
      clientUIntValue = value;
      if (clientUIntValue > 9999) { clientUIntValue = 9999; }

      int_to_str(clientUIntValue, numberStr, 5);
      numberStrLength = strlen(numberStr);

      display(caption);
    }

    // BE AWARE THAT this is used only for the battery trim field, which is a float between 0.0
    // and 2.0, with 3 digits after the dot, so we can use a fixed size string of 6 chars (1 for the
    // integer part, 1 for the dot, and 3 for the fraction part, plus the null terminator)
    auto show(double value, const char *caption) -> void {

      layout = Layout::FLOAT;

      clientFloatValue = value;
      if ((clientFloatValue >= 2.0) || (clientFloatValue <= 0.0)) { clientFloatValue = 1.0; }

      float_to_str(clientFloatValue, numberStr, 6, 3);

      numberStrLength = strlen(numberStr);

      // LOG_I("Digits: {}", numberStr);

      display(caption);
    }

    /**
     * @brief Event processing
     *
     * @param event Event coming from the user interaction
     * @return true The Keypad must keep control of events
     * @return false Events processing is complete
     */
    auto event(EventMgr::Event &event) -> bool {

      #if TOUCH_ENABLE
        LOG_I("Event: kind: {}, x: {}, y: {}, dist: {}", 
              EventMgr::eventStr[static_cast<uint8_t>(event.kind)],
              event.x, event.y, event.dist);
        if (event.kind == EventMgr::EventKind::TAP) {
          int8_t value = getKeyVal(event.x, event.y);
          LOG_I("Key value: {}", value);
          if (value != 99) {
            if (value >= 0) {
              addDigit(value);
            }
            else if (value == -1) {
              return false;
            }
            else if (value == -2) {
              if (layout == Layout::INT) {
                clientUIntValue = atoi(numberStr);
              } else {
                clientFloatValue = atof(numberStr);
              }
              return false;
            } else if (value == -3) {
              removeDigit();
            }
            else if (value == -4) {
              clearDigits();
            }
          }
        }
        page->start(fmt);
        updateValue();
        page->paint(false);

        return true;
      #else
        switch (event.kind) {
        #if EXTENDED_CASE || LINUX_BUILD
          case EventMgr::EventKind::PREV:
        #else
          case EventMgr::EventKind::DBL_PREV:
        #endif
            col        = (col == 0) ? 2 : col - 1;
            currentKey = matrix[line][col];
            break;

        #if EXTENDED_CASE || LINUX_BUILD
          case EventMgr::EventKind::NEXT:
        #else
          case EventMgr::EventKind::DBL_NEXT:
        #endif
            col        = (col == 2) ? 0 : col + 1;
            currentKey = matrix[line][col];
            break;

        #if EXTENDED_CASE || LINUX_BUILD
          case EventMgr::EventKind::DBL_PREV:
        #else
          case EventMgr::EventKind::PREV:
        #endif
            line       = (line == 0) ? 4 : line - 1;
            currentKey = matrix[line][col];
            break;

        #if EXTENDED_CASE || LINUX_BUILD
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
            if (value >= 0) {
              addDigit(value);
            }
            else if (value == -1) {
              return false;
            }
            else if (value == -2) {
              if (layout == Layout::INT) {
                clientUIntValue = atoi(numberStr);
              } else {
                clientFloatValue = atof(numberStr);
              }
              return false;
            } else if (value == -3) {
              removeDigit();
            }
            else if (value == -4) {
              clearDigits();
            }
          }
        } break;

        case EventMgr::EventKind::DBL_SELECT:
          event.kind = EventMgr::EventKind::SELECT;
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