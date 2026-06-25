// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "fonts.hpp"
#include "global.hpp"
#include "himem.hpp"
#include "himem_pool.hpp"
#include "picture.hpp"
#include "pugixml.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "display_list.hpp"
#include "models/css.hpp"

#define LINE_POS_TRACING 0

/**
 * @brief Page preparation
 *
 * This class supply all methods required to generate a page on the output screen.
 *
 * To prepare a page, the **start()** method is called. Following this call, the various methods
 * available in this class can be called to build the page display list. The **paint()** method can
 * then be called to push the display list to the screen.
 *
 */

using PagePtr = HimemUniquePtr<class Page>;

class Page {
  private:
    Page(Fonts &fonts) : fonts(fonts) {}

    Fonts &fonts;

  public:
    ~Page() = default; // { LOG_D("Page destructor called"); };

    template <typename T, typename ... Args>
    requires(!std::is_array_v<T>)
    friend auto makeUniqueHimem(Args &&... args)->HimemUniquePtr<T>;

    static inline auto Make(Fonts &fonts) { return makeUniqueHimem<Page>(fonts); }

    static const uint16_t HORIZONTAL_CENTER = 9999;

    struct Format {
      float lineHeightFactor           = 0.8; ///< In EMs
      int16_t fontIndex                = SYSTEM_REGULAR_FONT_INDEX;
      int16_t fontSize                 =  10; ///< In pixels
      int16_t indent                   =   0; ///< In pixels
      uint16_t marginLeft              =   0; ///< In pixels
      uint16_t marginRight             =   0; ///< In pixels
      uint16_t marginTop               =   0; ///< In pixels
      uint16_t marginBottom            =   0; ///< In pixels
      uint16_t screenLeft              =  10; ///< In pixels
      uint16_t screenRight             =  10; ///< In pixels
      uint16_t screenTop               =  10; ///< In pixels
      uint16_t screenBottom            =  10; ///< In pixels
      uint16_t width                   =  0; ///< In pixels
      uint16_t height                  =  0; ///< In pixels
      uint16_t verticalAlign           =  0; ///< In pixels
      bool trim                        = true;
      bool pre                         = false;
      FaceStyle fontStyle              = FaceStyle::NORMAL;
      CSS::Align align                 = CSS::Align::LEFT;
      CSS::TextTransform textTransform = CSS::TextTransform::NONE;
      CSS::Display display             = CSS::Display::INLINE;
    };

    /**
     * @brief Compute mode
     *
     * Used to select the level of processing made by the Page class to help
     * performance: LOCATION is used when computing the location of pages in
     * the EPUB document. No screen updates, no pictures, no glyphs are
     * rasterized. MOVE is used when it's time to travel from the beginning
     * of a book item (often related to chapters) to the beginning of the
     * page to be shown. DISPLAY is used when preparing the page to be shown
     * on screen.
     */
    enum class ComputeMode { LOCATION, MOVE, DISPLAY };

    struct EntityStringTag {};

  private:
    static constexpr char const *TAG = "Page";

    bool multiColumnMode{ false };
    uint8_t currentColumn{ 1 };
    uint8_t columnCount{ 1 };
    uint16_t columnWidth{ 0 };

    using EntityString =
      std::basic_string<char, std::char_traits<char>, StaticPoolAllocator<char, EntityStringTag> >;

    using Entities = std::unordered_map<
      EntityString, char32_t, std::hash<EntityString>, std::equal_to<EntityString>,
      StaticPoolAllocator<std::pair<const EntityString, char32_t>, EntityStringTag> >;

    static Entities entities;

    #if LINE_POS_TRACING
      bool tracing{ false };
    #endif

    /**
     * @brief Book Compute Mode
     *
     * This is used to indicate if we are computing the location of pages
     * to include in the database, moving to the start of a page to be
     * shown, or preparing a page to display on screen. This is mainly
     * used by the book_view and page classes to optimize the speed of
     * computations.
     */
    ComputeMode computeMode{ ComputeMode::DISPLAY };
    bool screenIsFull{ false }; ///< True if screen no more space to add characters
    bool pageEmpty{ true }; ///< True if no content has been added to the page

    DisplayListPool displayListPool{ 500 }; ///< Memory pool for DisplayListEntry objects, shared across
                                            ///< all DisplayList instances used by this Page instance

    DisplayListPtr displayList{ DisplayList::Make(displayListPool) };
    DisplayListPtr lineList{ DisplayList::Make(displayListPool) }; ///< Line preparation for paragraphs

    Pos pos;                      ///< Current drawing Screen position
    int16_t minY, maxX, maxY, minX; ///< Screen limits for page content
    int16_t paraMaxX, paraMinX;

    int16_t lineWidth, glyphsHeight;
    float lineHeightFactor;
    int16_t paraIndent, topMargin;

    auto addLine(const Format &fmt, bool justifyable) -> void;
    auto addGlyphToLine(Glyph *glyph, const Format &fmt, Font &font, bool isSpace) -> void;
    auto addPictureToLine(PicturePtr picture, int16_t advance, const Format &fmt) -> void;

  public:
    auto clean() -> void;

    #if LINE_POS_TRACING
      auto setTracing(bool value) -> void { tracing = value; }
    #endif

    /**
     * @brief Start a new page
     *
     * This is called to start drawing a new page. Position is reset to the top left location
     * on the screen. The parameters identify the limits in the screen where the characters will be
     * drawn.
     *
     * @param fmt Formatting parameters.
     */
    auto start(const Format &fmt, int8_t colCount = 1) -> void;

    /**
     * @brief Set the writing limits on a page without erasing
     *
     * Same behaviour as for the start() method, but without erasing the page. Used
     * to limit the location of rendering.
     *
     * @param fmt Formatting parameters.
     */
    auto setLimits(const Format &fmt) -> void;

    /**
     * @brief Start a new paragraph.
     *
     * @param fmt Formatting parameters.
     * @param recover True if it's the beginning of a page and in the middle of a paragraph.
     * @return true There is room for the beginning of a new paragraph.
     * @return false There is **no** room available for a new paragraph.
     */
    auto newParagraph(const Format &fmt, bool recover = false) -> bool;

    /**
     * @brief Signal a paragraph break
     *
     * Called when a paragraph is known to be split between two pages
     * and the last line on the page must be justified.
     *
     * @param fmt Formatting parameters.
     */
    auto breakParagraph(const Format &fmt) -> void;

    /**
     * @brief End the current paragraph
     *
     * @param fmt Formatting parameters.
     * @return true There is room for the end of paragraph.
     * @return false There is **no** room available the end of paragraph.
     */
    auto endParagraph(const Format &fmt) -> bool;

    /**
     * @brief Line Break
     *
     * A line break at the end of a page when there is no
     * additional space will be ignored.
     *
     * @param fmt Formatting parameters.
     * @param indentNextLine How many pixels to indent next line.
     * @return true The line break has been added to the paragraph.
     * @return false There is not enough space for a line break on page
     */
    auto lineBreak(const Format &fmt, int8_t indentNextLine = 0) -> bool;

    auto theScreenIsFull(const Format &fmt, FontPtr &font, uint16_t vertical_size_needed = 0) -> bool;


    /**
     * @brief Add a UTF-8 word to the paragraph.
     *
     * @param word The word to be added to the paragraph.
     * @param fmt Formatting parameters.
     * @return nullptr: The word has completely been added to the paragraph.
     * @return not nullptr: The internal word start address that was not put on page.
     *                      This is required has hyphen management has been added.
     */
    auto addWord(const char *word, const Format &fmt) -> const char *;

    /**
     * @brief Add a UTF-8 character to the paragraph.
     *
     * @param ch The UTF-8 character.
     * @param fmt Formatting parameters.
     * @return true The character has been added to the paragraph
     * @return false There is not enough room to add the character.
     */
    auto addChar(const char *ch, const Format &fmt) -> bool;

    /**
     * @brief Add an picture to the paragraph.
     *
     * @param picture Picture data. Each pixel is a grayscaled byte.
     * @param fmt Formatting parameters.
     * @param at_start_of_page True if it's the first item in a page
     * @return {true, nullptr} The picture has been added to the paragraph
     * @return {false, picture} There is not enough room to add the picture. The picture is returned
     * to the caller.
     */
    auto addPicture(PicturePtr picture, const Format &fmt /*, bool at_start_of_page*/)
    -> std::pair<bool, PicturePtr>;

    /**
     * @brief Add text on page
     *
     * Simple algorithm to add text on a page at current location.
     * This is used by the books_dir_view class. It is expected that
     * the text will fit inside the page.
     *
     * @param str Text to show
     * @param fmt Formatting parameters.
     */
    auto addText(const std::string &str, const Format &fmt) -> void;

    /**
     * @brief Put string to the screen.
     *
     * This method put the string at a specific screen location.
     *
     * @param str
     * @param pos If pos.x == -1, use screen margin positions
     * @param fmt Formatting parameters.
     */
    auto putStrAt(const std::string &str, Pos pos, const Format &fmt) -> void;

    /**
     * @brief Put character to the screen.
     *
     * This method put a character at a specific screen location.
     *
     * @param ch
     * @param pos If pos.x == -1, use screen margin positions
     * @param fmt Formatting parameters.
     */
    auto putCharAt(char ch, Pos pos, const Format &fmt) -> void;

    /**
     * @brief Paint the display list to the screen.
     *
     * The screen is first erased and the painting process is done using
     * the content of the display list.
     *
     * @param clearScreen Screen contain is erased before painting.
     * @param noFull      Bypass partial update count control. Use with great caution!
     * @param doIt        Do the painting irrelevant of the compute mode
     */
    auto paint(bool clearScreen = true, bool noFull = false, bool doIt = false) -> void;

    auto showFmt(const Format &fmt, const char *spaces) -> void const {

      //#if DEBUGGING
      std::cout << spaces << "Fmt: align:" << (int)fmt.align << " valign:" << (int)fmt.verticalAlign
                << " Idx:" << fmt.fontIndex << " Sz:" << fmt.fontSize << " St:" << (int)fmt.fontStyle
                << " ind:" << fmt.indent << " lhf:" << fmt.lineHeightFactor
                << " m:" << fmt.marginBottom << "," << fmt.marginLeft << "," << fmt.marginRight << ","
                << fmt.marginTop << " s:" << fmt.screenBottom << "," << fmt.screenLeft << ","
                << fmt.screenRight << "," << fmt.screenTop << " tr:" << fmt.trim << " pr:" << fmt.pre
                << " tt:" << (int)fmt.textTransform << " di:" << (int)fmt.display << std::endl;
      //#endif
    }

    auto showCover(PicturePtr &pict) -> bool;
    auto putPicture(PicturePtr picture, Pos pos) -> void;
    auto putHighlight(Dim dim, Pos pos) -> void;
    auto clearHighlight(Dim dim, Pos pos) -> void;
    auto putRounded(Dim dim, Pos pos) -> void;
    auto clearRounded(Dim dim, Pos pos) -> void;
    auto clearRegion(Dim dim, Pos pos) -> void;
    auto setRegion(Dim dim, Pos pos) -> void;

    auto showControls(const char *spaces) -> void const {
      #if DEBUGGING
        std::cout << spaces << " pos.x:" << pos.x << " pos.y:" << pos.y << " minX:" << minX
                  << " maxX:" << maxX << " minY:" << minY << " maxY:" << maxY
                  << " paraMinX:" << paraMinX << " paraMaxX:" << paraMaxX << " indent:" << paraIndent
                  << " width:" << lineWidth << std::endl;
      #endif
    }

    auto toUnicode(const char *str, CSS::TextTransform transform, bool first, const char **str2) const
    -> char32_t;

    inline auto setComputeMode(ComputeMode mode) -> void { computeMode = mode; }

    [[nodiscard]] inline auto getComputeMode() const -> ComputeMode { return computeMode; }
    [[nodiscard]] inline auto paintWidth() const -> int16_t { return maxX - minX; }
    [[nodiscard]] inline auto isFull() const -> bool { return screenIsFull; }
    [[nodiscard]] inline auto isEmpty() const -> bool { return pageEmpty; }
    [[nodiscard]] inline auto someDataWaiting() const -> bool { return !lineList->empty(); }
    [[nodiscard]] inline auto getDisplayList() const -> const DisplayList & { return *displayList; }
    [[nodiscard]] inline auto getLineList() const -> const DisplayList & { return *lineList; }
    [[nodiscard]] inline auto getPosY() const -> int16_t { return pos.y; }

    auto getPixelValue(const CSS::Value &value, const Format &fmt, int16_t ref, bool vertical = false)
    -> int16_t;
    auto getPointValue(const CSS::Value &value, const Format &fmt, int16_t ref) -> int16_t;
    auto getFactorValue(const CSS::Value &value, const Format &fmt, float ref) -> float;
    void adjustFormat(DOM::Node *domCurrentNode, Format &fmt, const CSSPtr &elementCss,
                      const CSSPtr &itemCss);
    auto adjustFormatFromRules(Format &fmt, const CSS::RulesMap &rules) -> void;

    inline auto resetFontIndex(Format &fmt, FaceStyle style) -> void {
      if (style != fmt.fontStyle) {
        int16_t idx = -1;
        if ((idx = fonts.getFontIndex(fonts.getName(fmt.fontIndex), style)) == -1) {
          // LOG_E("Font not found 2: {} {}", fonts.getName(fmt.fontIndex), style);
          idx = fonts.getFontIndex("Default", style);
        }
        if (idx == -1) {
          fmt.fontStyle = FaceStyle::NORMAL;
          fmt.fontIndex = 3;
        } else {
          fmt.fontStyle = style;
          fmt.fontIndex = idx;
        }
      }
    }
};
