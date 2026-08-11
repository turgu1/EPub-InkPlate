// Keyboard Viewer Specifications
//
// The keyboard viewer is a component that allows users to input text using an on-screen keyboard.
// The keyboard viewer must support both touch and button events, adapting to the screen size 
// and providing a user-friendly interface for text entry similar to the keypad viewer.
// The following specifications outline the requirements and behavior of the keyboard viewer:
//
// 1. The keyboard viewer must receive a caption and a reference to a string where the value will be retrieved. 
//    The string must be updated with the value entered by the user when the OK button is pressed. 
//    The string must contains at most 32 characters and may contains ASCII characters to be displayed. 
//    Support for entering UTF-8 characters will be implemented in a later version of the keyboard 
//    viewer. For now, only ASCII characters will be supported for entering by the user.
// 2. The keyboard must adapt to the screen width minus 20 pixels and located at the center of the screen,
//    with a maximum width of 600 pixels. The keyboard must be centered vertically on the screen.
// 3. The interaction of the user with the keyboard must be similar to the keypad viewer, supporting
//    both touch and buttons events.
// 4. The value must be displayed on top of the keyboard in a rectangle as large as the keyboard similar 
//    to the keypad viewer. A label must be displayed on top of the rectangle with the caption 
//    of the field received as a parameter. The value must be centered in the rectangle. 
//    The rectangle must be rounded with a radius of 8 pixels and a thickness of 3.
// 5. All keyboard keys must be rounded with a radius of 8 pixels.
// 6. The shift key must be highlighted (-1) when the keyboard is in the shifted state.
// 7. The shifted state must remain until the SHIFT key is pressed again or a change from the ALFA keyboard to the numeric keyboard is done. Same from the numeric to the alpha keyboard.
// 8. When the OK button is pressed, the value must me returned to the calling method.
// 9. The '*' key close to the 'OK' key is to hide the resulting string using * characters for password 
//    entry purpose. A 'bool hide' parameter on the show() method is used to set the keyboard in hide mode.
//    When in hide mode, the '*' key must stay highlighted until the user press the key to see the
//    string content. Pressing the key again will hide the string, the key being kept as higlighted until the next
//    time the user will press it. This behavior must be consistent in the 4 mappings.
//
// Here are the Keyboard Viewer 4 layouts diagrams:
//
// Alphabetical Non-Shifted:
// 
// +---+---+---+---+---+---+---+---+---+---+
// | q | w | e | r | t | y | u | i | o | p |
// +---+---+---+---+---+---+---+---+---+---+
//   | a | s | d | f | g | h | j | k | l |
// +-+---+---+---+---+---+---+---+---+---+-+
// |SHIFT| z | x | c | v | b | n | m | DEL |
// +-----+---+---+---+---+---+---+---+-----+ 
// | 123 |        SPACE      | . | * |  OK |
// +-----+-------------------+---+---+-----+
// 
// Alphabetical Shifted:
// 
// +---+---+---+---+---+---+---+---+---+---+
// | Q | W | E | R | T | Y | U | I | O | P |
// +---+---+---+---+---+---+---+---+---+---+
//   | A | S | D | F | G | H | J | K | L |
// +-+---+---+---+---+---+---+---+---+---+-+
// |SHIFT| Z | X | C | V | B | N | M | DEL |
// +-----+---+---+---+---+---+---+---+-----+ 
// | 123 |        SPACE      | . | * |  OK |
// +-----+-------------------+---+---+-----+
// 
// Numerical Non-Shifted:
// 
// +---+---+---+---+---+---+---+---+---+---+
// | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 |
// +---+---+---+---+---+---+---+---+---+---+
// | @ | # | $ | % | ^ | & | * | ( | ) | _ |
// +-+---+---+---+---+---+---+---+---+---+-+
// |SHIFT| - | = | + | [ | ] | { | } | DEL |
// +-----+---+---+---+---+---+---+---+-----+ 
// | ABC |        SPACE      | . | * | OK  |
// +-----+-------------------+---+---+-----+
// 
// Numerical Shifted:
// 
// +---+---+---+---+---+---+---+---+---+---+
// | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 |
// +---+---+---+---+---+---+---+---+---+---+
// | @ | # | $ | ~ | ` | " | ' | < | > | : |
// +-+---+---+---+---+---+---+---+---+---+-+
// |SHIFT| ! | ? | | | / | \ | , | ; | DEL |
// +-----+---+---+---+---+---+---+---+-----+ 
// | ABC |        SPACE      | . | * | OK  |
// +-----+-------------------+---+---+-----+

#pragma once
#include "global.hpp"
#include "himem.hpp"
#include "controllers/event_mgr.hpp"
#include "fonts.hpp"
#include "viewers/page.hpp"

using KeyboardViewerPtr = HimemUniquePtr<class KeyboardViewer>;

class KeyboardViewer {
  private:
    KeyboardViewer() = default;
    PagePtr page{ Page::Make(appFonts) };

    enum class Layout { ALPHA_LOW, ALPHA_UP, NUM_NORM, NUM_SHIFT };
    Layout currentLayout{ Layout::ALPHA_LOW };

    static const uint8_t MAX_LEN = 32, FONT_SIZE = 9, KEY_COUNT = 34;
    static const int8_t VAL_SHIFT = -1, VAL_DEL = -2, VAL_TOGGLE = -3, VAL_SPACE = -4, VAL_OK = -5, VAL_PASS = -6;

    struct KeyLocation { Dim dim; Pos pos; int8_t value; const char* label; };
    KeyLocation keyLocs[KEY_COUNT];
    Dim kbDim, keyDim;
    Pos kbPos, fieldPos;
    char textStr[MAX_LEN + 1];
    uint8_t textLen{0};
    uint8_t activeMaxLen{MAX_LEN}; 
    std::string *clientStr{nullptr};
    Page::Format fmt{};
    Glyph *glyph;
    const char* activeCaption{nullptr};
    bool isPasswordHidden{false}; 

#if !TOUCH_ENABLE
    // Fixed: Explicit, fixed-size 2D array of pointers to handle all matrix[line][col] assignments natively
    KeyLocation* matrix[4][10]; 
    KeyLocation *currentKey{nullptr}, *previousKey{nullptr};
    uint8_t line{0}, col{0};
    uint8_t rowLimits[4] = {10, 10, 10, 10}; 
#endif

    const char* layouts[4][KEY_COUNT] = {
        // ALPHA_LOW
        {"q","w","e","r","t","y","u","i","o","p",
         "a","s","d","f","g","h","j","k","l","",
         "SHIFT","z","x","c","v","b","n","m","DEL",
         "123","SPACE",".","*","OK"},
        // ALPHA_UP
        {"Q","W","E","R","T","Y","U","I","O","P",
         "A","S","D","F","G","H","J","K","L","",
         "SHIFT","Z","X","C","V","B","N","M","DEL",
         "123","SPACE",".","*","OK"},
        // NUM_NORM
        {"1","2","3","4","5","6","7","8","9","0",
         "@","#","$","%","^","&","*","Scope(","Scope)","_",
         "SHIFT","-","=","+","[","]","{","}","DEL",
         "ABC","SPACE",".","*","OK"},
        // NUM_SHIFT
        {"1","2","3","4","5","6","7","8","9","0",
         "@","#","$","~","`","\"","'","<",">",":",
         "SHIFT","!","?","|","/","\\",",",";","DEL",
         "ABC","SPACE",".","*","OK"}
    };

    const char* getLabel(uint8_t idx) { 
        return layouts[static_cast<uint8_t>(currentLayout)][idx]; 
    }

    void updateValue() {
        page->clearRegion(Dim(kbDim.width - 10, keyDim.height + 6), Pos(fieldPos.x + 5, fieldPos.y + 5));
        
        FontPtr &largeFont = appFonts.getFont(2);
        uint8_t largeFontSize = FONT_SIZE + 2;
        Glyph *largeGlyph = largeFont->getGlyph('0', largeFontSize);
        
        Page::Format largeFmt = fmt;
        largeFmt.fontSize = largeFontSize;
        largeFmt.align = HAlign::CENTER;

        uint16_t maxPixelWidth = kbDim.width - 24; 
        uint8_t startIndex = 0;
        uint8_t currentLen = textLen;

        Dim strDim = largeFont->getASCIISize(textStr, largeFontSize);
        
        if (strDim.width > maxPixelWidth && textLen > 0) {
            for (uint8_t i = 1; i < textLen; ++i) {
                Dim trailingDim = largeFont->getASCIISize(&textStr[i], largeFontSize);
                if (trailingDim.width <= maxPixelWidth) {
                    startIndex = i;
                    currentLen = textLen - i;
                    break;
                }
            }
            if (startIndex == 0) {
                startIndex = textLen - 1;
                currentLen = 1;
            }
        }

        char const* displayPtr = &textStr[startIndex];

        if (isPasswordHidden) {
            char maskStr[MAX_LEN + 1];
            std::memset(maskStr, '*', currentLen);
            maskStr[currentLen] = 0;
            page->putStrAt(maskStr, Pos(fieldPos.x + (kbDim.width >> 1), fieldPos.y + (largeGlyph->dim.height >> 1) + ((keyDim.height + 16) >> 1)), largeFmt);
        } else {
            page->putStrAt(displayPtr, Pos(fieldPos.x + (kbDim.width >> 1), fieldPos.y + (largeGlyph->dim.height >> 1) + ((keyDim.height + 16) >> 1)), largeFmt);
        }
    }

    void drawButton(const KeyLocation &k, int8_t thickness = 1) {
        if (k.value == VAL_SHIFT && (currentLayout == Layout::ALPHA_UP || currentLayout == Layout::NUM_SHIFT)) thickness = -1;
        if (k.value == VAL_PASS && isPasswordHidden) thickness = -1; 
        #if !TOUCH_ENABLE
          if (currentKey == &k) thickness = -1;
        #endif
        if (!k.label || k.label[0] == '\0') return;

        if (thickness < 0) fmt.fontStyle = FaceStyle::INVERSE;
        page->putRounded(k.dim, k.pos, thickness, 8);
        
        Page::Format bf = fmt; bf.align = HAlign::CENTER;
        page->putStrAt(k.label, Pos(k.pos.x + (k.dim.width >> 1), k.pos.y + (glyph->dim.height >> 1) + (k.dim.height >> 1)), bf);
        fmt.fontStyle = FaceStyle::NORMAL;
    }

#if !TOUCH_ENABLE
    void updateHighlight() {
        if (previousKey != currentKey) {
            if (previousKey && previousKey->label && previousKey->label[0] != '\0') { 
                page->clearRounded(Dim(previousKey->dim.width - 2, previousKey->dim.height - 2), Pos(previousKey->pos.x + 1, previousKey->pos.y + 1), -1, 8); 
                drawButton(*previousKey, 1); 
            }
            if (currentKey && currentKey->label && currentKey->label[0] != '\0') {
                page->clearRounded(Dim(currentKey->dim.width - 2, currentKey->dim.height - 2), Pos(currentKey->pos.x + 1, currentKey->pos.y + 1), -1, 8); 
                drawButton(*currentKey, -1);
            }
        }
    }
#endif
    void drawWholeKeyboardFrame() {
        page->start(fmt);
        page->clearRegion(Dim(kbDim.width + 26, kbDim.height + 36), Pos(kbPos.x - 13, kbPos.y - 13));
        page->putRounded(Dim(kbDim.width + 20, kbDim.height + 30), Pos(kbPos.x - 10, kbPos.y - 10), 2, 8);
        
        Page::Format cf = fmt; cf.align = HAlign::CENTER;
        page->putStrAt(activeCaption, Pos(Screen::getWidth() >> 1, kbPos.y + glyph->dim.height + 10), cf);
        page->putRounded(Dim(kbDim.width, keyDim.height + 16), Pos(fieldPos.x, fieldPos.y), 3, 8);

#if !TOUCH_ENABLE
        std::memset(matrix, 0, sizeof(matrix));
#endif
        uint16_t rY = fieldPos.y + keyDim.height + 22, kIdx = 0;
        bool isAlpha = (currentLayout == Layout::ALPHA_LOW || currentLayout == Layout::ALPHA_UP);

        // Row 0: 10 Keys
        for (uint8_t c = 0; c < 10; ++c) {
            keyLocs[kIdx] = { keyDim, Pos(kbPos.x + (keyDim.width + 2) * c, rY), static_cast<int8_t>(kIdx), getLabel(kIdx) };
#if !TOUCH_ENABLE
            matrix[0][c] = &keyLocs[kIdx];
#endif
            kIdx++;
        }

        // Row 1: Indented Row
        rY += keyDim.height + 2;
        uint16_t indentPadding = ((kbDim.width - ((keyDim.width * 9) + 16)) >> 1);
        for (uint8_t c = 0; c < 10; ++c) {
            if (c == 9) {
                Dim padDim = isAlpha ? Dim(0, 0) : keyDim;
                Pos padPos = isAlpha ? Pos(0, 0) : Pos(kbPos.x + (keyDim.width + 2) * 9, rY);
                keyLocs[kIdx] = { padDim, padPos, isAlpha ? (int8_t)99 : static_cast<int8_t>(kIdx), getLabel(kIdx) };
#if !TOUCH_ENABLE
                matrix[1][c] = isAlpha ? nullptr : &keyLocs[kIdx];
#endif
                kIdx++;
            } else {
                uint16_t finalX = isAlpha ? (kbPos.x + indentPadding + (keyDim.width + 2) * c) : (kbPos.x + (keyDim.width + 2) * c);
                keyLocs[kIdx] = { keyDim, Pos(finalX, rY), static_cast<int8_t>(kIdx), getLabel(kIdx) };
#if !TOUCH_ENABLE
                matrix[1][c] = &keyLocs[kIdx];
#endif
                kIdx++;
            }
        }

        // Row 2: SHIFT, 7 Keys, DEL
        rY += keyDim.height + 2;
        Dim shiftDim = Dim((keyDim.width * 3) >> 1, keyDim.height);
        keyLocs[kIdx] = { shiftDim, Pos(kbPos.x, rY), VAL_SHIFT, getLabel(kIdx) };
#if !TOUCH_ENABLE
        matrix[2][0] = &keyLocs[kIdx];
        matrix[2][1] = &keyLocs[kIdx];
#endif
        uint16_t cX = kbPos.x + shiftDim.width + 2; kIdx++;

        for (uint8_t c = 2; c <= 8; ++c) {
            keyLocs[kIdx] = { keyDim, Pos(cX, rY), static_cast<int8_t>(kIdx), getLabel(kIdx) };
#if !TOUCH_ENABLE
            matrix[2][c] = &keyLocs[kIdx];
#endif
            cX += keyDim.width + 2; kIdx++;
        }
        Dim delDim = Dim((kbPos.x + kbDim.width) - cX, keyDim.height);
        keyLocs[kIdx] = { delDim, Pos(cX, rY), VAL_DEL, getLabel(kIdx) };
#if !TOUCH_ENABLE
        matrix[2][9] = &keyLocs[kIdx];
#endif
        kIdx++;

        // Row 3: TOGGLE, SPACE, '.', '*', OK
        rY += keyDim.height + 2;
        keyLocs[kIdx] = { shiftDim, Pos(kbPos.x, rY), VAL_TOGGLE, getLabel(kIdx) };
#if !TOUCH_ENABLE
        matrix[3][0] = &keyLocs[kIdx];
        matrix[3][1] = &keyLocs[kIdx];
#endif
        cX = kbPos.x + shiftDim.width + 2; kIdx++;

        uint16_t elementsRight = (keyDim.width * 2) + ((keyDim.width * 3) >> 1) + 6;
        Dim spaceDim = Dim((kbPos.x + kbDim.width) - cX - elementsRight - 2, keyDim.height);
        keyLocs[kIdx] = { spaceDim, Pos(cX, rY), VAL_SPACE, getLabel(kIdx) };
#if !TOUCH_ENABLE
        matrix[3][2] = &keyLocs[kIdx];
        matrix[3][3] = &keyLocs[kIdx];
        matrix[3][4] = &keyLocs[kIdx];
        matrix[3][5] = &keyLocs[kIdx];
        matrix[3][6] = &keyLocs[kIdx];
#endif
        cX += spaceDim.width + 2; kIdx++;

        keyLocs[kIdx] = { keyDim, Pos(cX, rY), static_cast<int8_t>(kIdx), getLabel(kIdx) };
#if !TOUCH_ENABLE
        matrix[3][7] = &keyLocs[kIdx];
#endif
        cX += keyDim.width + 2; kIdx++;

        keyLocs[kIdx] = { keyDim, Pos(cX, rY), VAL_PASS, getLabel(kIdx) };
#if !TOUCH_ENABLE
        matrix[3][8] = &keyLocs[kIdx];
#endif
        cX += keyDim.width + 2; kIdx++;

        Dim okDim = Dim((kbPos.x + kbDim.width) - cX, keyDim.height);
        keyLocs[kIdx] = { okDim, Pos(cX, rY), VAL_OK, getLabel(kIdx) };
#if !TOUCH_ENABLE
        matrix[3][9] = &keyLocs[kIdx];
#endif

#if !TOUCH_ENABLE
        currentKey = matrix[line][col];
#endif

        for (uint8_t i = 0; i < KEY_COUNT; ++i) {
            drawButton(keyLocs[i], 1);
        }

        updateValue();
        page->paint(false);
    }
    bool processKey(int8_t idx) {
        if (idx == 99) return true;
        const auto &k = keyLocs[idx];
        if (k.value >= 0) {
            if (textLen < activeMaxLen && k.label && k.label[0] != '\0') { 
                textStr[textLen++] = k.label[0]; textStr[textLen] = 0; 
            }
        } else {
            switch (k.value) {
                case VAL_SHIFT:
                    if (currentLayout == Layout::ALPHA_LOW) currentLayout = Layout::ALPHA_UP;
                    else if (currentLayout == Layout::ALPHA_UP) currentLayout = Layout::ALPHA_LOW;
                    else if (currentLayout == Layout::NUM_NORM) currentLayout = Layout::NUM_SHIFT;
                    else if (currentLayout == Layout::NUM_SHIFT) currentLayout = Layout::NUM_NORM;
                    break;
                case VAL_DEL: if (textLen > 0) textStr[--textLen] = 0; break;
                case VAL_SPACE: if (textLen < activeMaxLen) { textStr[textLen++] = ' '; textStr[textLen] = 0; } break;
                case VAL_TOGGLE: currentLayout = (currentLayout == Layout::ALPHA_LOW || currentLayout == Layout::ALPHA_UP) ? Layout::NUM_NORM : Layout::ALPHA_LOW; break;
                case VAL_PASS: isPasswordHidden = !isPasswordHidden; break;
                case VAL_OK: if (clientStr) *clientStr = textStr; return false;
            }
        }
#if !TOUCH_ENABLE
        rowLimits[0] = 10; rowLimits[1] = 10; rowLimits[2] = 10; rowLimits[3] = 10;
#endif
        drawWholeKeyboardFrame();
        return true;
    }

  public:
    template <typename T, typename ... Args> requires(!std::is_array_v<T>) friend auto makeUniqueHimem(Args &&... args)->HimemUniquePtr<T>;
    static inline auto Make() { return makeUniqueHimem<KeyboardViewer>(); }
    ~KeyboardViewer() = default;

    void show(std::string &value, uint8_t maxLength, const char *caption, bool hide = false) {
        clientStr = &value; currentLayout = Layout::ALPHA_LOW; activeCaption = caption; isPasswordHidden = hide;
        
        activeMaxLen = maxLength;
        if (activeMaxLen > MAX_LEN) activeMaxLen = MAX_LEN;
        if (activeMaxLen == 0) activeMaxLen = MAX_LEN;

        textLen = std::min((size_t)activeMaxLen, value.length());
        std::memcpy(textStr, value.c_str(), textLen); textStr[textLen] = 0;

        glyph = appFonts.getFont(1)->getGlyph('0', FONT_SIZE);
        uint16_t outW = Screen::getWidth() - 50; 
        if (outW > 600) { outW = 600; } 
        
        keyDim = Dim((outW - 18) / 10, glyph->dim.height + 20);
        kbDim = Dim(outW, (keyDim.height + 16) + 36 + ((keyDim.height + 2) * 4));
        kbPos = Pos((Screen::getWidth() >> 1) - (kbDim.width >> 1), (Screen::getHeight() >> 1) - (kbDim.height >> 1));
        
        fieldPos = Pos(kbPos.x, kbPos.y + 40);
        fmt = { .fontSize = FONT_SIZE, .marginLeft = 5, .marginRight = 5, .screenLeft = 20, .screenRight = 20, .align = HAlign::CENTER };

#if !TOUCH_ENABLE
        line = 1; 
        col = 4;
        previousKey = nullptr; 
        currentKey = nullptr;
#endif
        drawWholeKeyboardFrame();
#if !TOUCH_ENABLE
        previousKey = nullptr; 
        currentKey = matrix[line][col]; 
        updateHighlight();
        previousKey = currentKey;
#endif
    }

    auto event(EventMgr::Event &event) -> bool {
#if TOUCH_ENABLE
        if (event.kind == EventMgr::EventKind::TAP) {
            for (int i = 0; i < KEY_COUNT; i++) {
                if (keyLocs[i].dim.width > 0 && (event.x >= keyLocs[i].pos.x) && (event.x <= keyLocs[i].pos.x + keyLocs[i].dim.width) &&
                    (event.y >= keyLocs[i].pos.y) && (event.y <= keyLocs[i].pos.y + keyLocs[i].dim.height)) {
                    if (!processKey(i)) { page->clearRegion(Dim(kbDim.width + 26, kbDim.height + 36), Pos(kbPos.x - 13, kbPos.y - 13)); return false; }
                    break;
                }
            }
        }
        page->start(fmt); updateValue(); page->paint(false); return true;
#else
        switch (event.kind) {
            case EventMgr::EventKind::PREV: {
                KeyLocation *startKey = matrix[line][col];
                do { 
                    col = (col == 0) ? 9 : col - 1; 
                } while (matrix[line][col] == nullptr || matrix[line][col] == startKey);
                break;
            }
            case EventMgr::EventKind::NEXT: {
                KeyLocation *startKey = matrix[line][col];
                do { 
                    col = (col >= 9) ? 0 : col + 1; 
                } while (matrix[line][col] == nullptr || matrix[line][col] == startKey);
                break;
            }
            case EventMgr::EventKind::DBL_PREV: 
                line = (line == 0) ? 3 : line - 1; col = std::min(col, (uint8_t)9);
                while(matrix[line][col] == nullptr) { col--; }
                break;
            case EventMgr::EventKind::DBL_NEXT: 
                line = (line == 3) ? 0 : line + 1; col = std::min(col, (uint8_t)9);
                while(matrix[line][col] == nullptr) { col--; }
                break;
            case EventMgr::EventKind::SELECT:
                for (uint8_t i = 0; i < KEY_COUNT; ++i) {
                    if (&keyLocs[i] == currentKey) { if (!processKey(i)) { page->clearRegion(Dim(kbDim.width + 26, kbDim.height + 36), Pos(kbPos.x - 13, kbPos.y - 13)); return false; } break; }
                }
                break;
            default: break;
        }
        currentKey = matrix[line][col];
        page->start(fmt); updateValue(); updateHighlight(); previousKey = currentKey; page->paint(false); return true;
#endif
    }
};
