// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include <string>
#include <forward_list>

#include "global.hpp"
#include "models/image.hpp"
#include "models/fonts.hpp"
#include "models/css.hpp"
#include "memory_pool.hpp"

#include "pugixml.hpp"

/**
 * @brief Page preparation
 * 
 * This class supply all methods required to generate a page on the output screen.
 * 
 * To prepare a page, the **start()** method is called. Following this call, the various methods
 * available in this class can be called to build the page display list. The **paint()** method can then
 * be called to push the display list to the screen.
 *  
 */
class Page
{
  public:
    struct Format {
      float              line_height_factor; ///< In EMs
      int16_t            font_index;
      int16_t            font_size;          ///< In pixels
      int16_t            indent;             ///< In pixels
      int16_t            margin_left;        ///< In pixels
      int16_t            margin_right;       ///< In pixels
      int16_t            margin_top;         ///< In pixels
      int16_t            margin_bottom;      ///< In pixels
      int16_t            screen_left;        ///< In pixels
      int16_t            screen_right;       ///< In pixels
      int16_t            screen_top;         ///< In pixels
      int16_t            screen_bottom;      ///< In pixels
      int16_t            width;              ///< In pixels
      int16_t            height;             ///< In pixels
      int16_t            vertical_align;     ///< In pixels
      bool               trim;
      bool               pre;
      Fonts::FaceStyle   font_style;
      CSS::Align         align;
      CSS::TextTransform text_transform;
      CSS::Display       display;
    };

    /**
     * @brief Compute mode
     * 
     * Used to select the level of processing made by the Page class to help
     * performance: LOCATION is used when computing the location of pages in
     * the EPUB document. No screen updates, no images, no glyphs are 
     * rasterized. MOVE is used when it's time to travel from the beginning 
     * of a book item (often related to chapters) to the beginning of the 
     * page to be shown. DISPLAY is used when preparing the page to be shown
     * on screen. 
     */
    enum class ComputeMode { LOCATION, MOVE, DISPLAY };

  private:
    static constexpr char const * TAG = "Page";

    enum class DisplayListCommand { GLYPH = 1, IMAGE, HIGHLIGHT, CLEAR_HIGHLIGHT, CLEAR_REGION, SET_REGION, ROUNDED, CLEAR_ROUNDED };
    struct DisplayListEntry {
      union Kind {
        struct GryphEntry {            ///< Used for GLYPH
          TTF::BitmapGlyph * glyph;    ///< Glyph
          bool is_space;
        } glyph_entry;
        struct ImageEntry {            ///< Used for IMAGE
          Image::ImageData image;       
          int16_t advance;             ///< Horizontal advance on the baseline
        } image_entry;
        struct RegionEntry {           ///< Used for HIGHLIGHT, CLEAR_HIGHLIGHT, SET_REGION and CLEAR_REGION
          Dim dim;                     ///< Region dimensions
        } region_entry;
        Kind() {}
      } kind;
      Pos pos;                         ///< Screen coordinates
      DisplayListCommand command;      ///< Command
    };

    typedef std::forward_list<DisplayListEntry *> DisplayList;

    /**
     * @brief Book Compute Mode
     * 
     * This is used to indicate if we are computing the location of pages 
     * to include in the database, moving to the start of a page to be
     * shown, or preparing a page to display on screen. This is mainly 
     * used by the book_view and page classes to optimize the speed of 
     * computations.
     */
    ComputeMode compute_mode;

    MemoryPool <DisplayListEntry> display_list_entry_pool;

    DisplayList display_list;            ///< The list of artefacts and their position to put on screen
    DisplayList line_list;               ///< Line preparation for paragraphs

    bool screen_is_full;                 ///< True if screen no more space to add characters

    Pos     pos;                         ///< Current drawing Screen position
    int16_t min_y, max_x, max_y, min_x;  ///< Screen limits for page content
    int16_t para_max_x, para_min_x;

    int16_t line_width,  glyphs_height;
    float   line_height_factor;
    int16_t para_indent, top_margin;

    // Entries of a line list are eventually migrated to the display_list. 
    // So the don't need to be erased.  
    inline void clear_line_list() { line_list.clear(); }

    void clear_display_list();
    void           add_line(const Format & fmt, bool justifyable);
    void  add_glyph_to_line(TTF::BitmapGlyph * glyph, const Format & fmt, TTF & font, bool is_space);
    void  add_image_to_line(Image & image, int16_t advance, const Format & fmt);
    int32_t      to_unicode(const char **str, CSS::TextTransform transform, bool first) const;

  public:

    Page();
    void clean();

    /**
     * @brief Start a new page
     * 
     * This is called to start drawing a new page. Position is reset to the top left location
     * on the screen. The parameters identify the limits in the screen where the characters will be
     * drawn.
     * 
     * @param fmt Formatting parameters.
     */
    void start(Format & fmt);

    /**
     * @brief Set the writing limits on a page without erasing
     * 
     * Same behaviour as for the start() method, but without erasing the page. Used
     * to limit the location of rendering.
     * 
     * @param fmt Formatting parameters.
     */
    void set_limits(Format & fmt);

    /**
     * @brief Start a new paragraph.
     * 
     * @param fmt Formatting parameters.
     * @param recover True if it's the beginning of a page and in the middle of a paragraph.
     * @return true There is room for the beginning of a new paragraph.
     * @return false There is **no** room available for a new paragraph.
     */
    bool new_paragraph(const Format & fmt, bool recover = false);

    /**
     * @brief Signal a paragraph break
     * 
     * Called when a paragraph is known to be split between two pages
     * and the last line on the page must be justified.
     *
     * @param fmt Formatting parameters.
     */ 
    void break_paragraph(const Format & fmt);

    /**
     * @brief End the current paragraph
     * 
     * @param fmt Formatting parameters.
     * @return true There is room for the end of paragraph.
     * @return false There is **no** room available the end of paragraph.
     */
    bool end_paragraph(const Format & fmt);

    /**
     * @brief Line Break
     * 
     * A line break at the end of a page when there is no
     * additional space will be ignored.
     * 
     * @param fmt Formatting parameters.
     * @param indent_next_line How many pixels to indent next line.
     * @return true The line break has been added to the paragraph.
     * @return false There is not enough space for a line break on page
     */
    bool line_break(const Format & fmt, int8_t indent_next_line = 0);

    /**
     * @brief Add a UTF-8 word to the paragraph.
     *
     * @param word The word to be added to the paragraph.
     * @param fmt Formatting parameters.
     * @return true The word has been added to the paragraph.
     * @return false There is not enough space to add the word on page.
     */
    bool add_word(const char * word, const Format & fmt);

    /**
     * @brief Add a UTF-8 character to the paragraph.
     * 
     * @param ch The UTF-8 character.
     * @param fmt Formatting parameters.
     * @return true The character has been added to the paragraph
     * @return false There is not enough room to add the character.
     */
    bool add_char(const char * ch, const Format & fmt);

    /**
     * @brief Add an image to the paragraph.
     * 
     * @param image Image data. Each pixel is a grayscaled byte.
     * @param fmt Formatting parameters.
     * @param at_start_of_page True if it's the first item in a page
     * @return true The image has been added to the paragraph
     * @return false There is not enough room to add the image.
     */
    bool add_image(Image & image, const Format & fmt /*, bool at_start_of_page*/);

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
    void add_text(std::string str, const Format & fmt);

    /**
     * @brief Put string to the screen.
     * 
     * This method put the string at a specific screen location.
     * 
     * @param str 
     * @param pos If pos.x == -1, use screen margin positions
     * @param fmt Formatting parameters.
     */
    void put_str_at(const std::string & str, Pos pos, const Format & fmt);

    /**
     * @brief Put character to the screen.
     * 
     * This method put a character at a specific screen location.
     * 
     * @param ch 
     * @param pos If pos.x == -1, use screen margin positions
     * @param fmt Formatting parameters.
     */
    void put_char_at(char ch, Pos pos, const Format & fmt);

    /**
     * @brief Paint the display list to the screen.
     * 
     * The screen is first erased and the painting process is done using 
     * the content of the display list.
     * 
     * @param clear_screen Screen contain is erased before painting.
     * @param no_full      Bypass partial update count control. Use with great caution!
     * @param do_it        Do the painting irrelevant of the compute mode
     */
    void paint(bool clear_screen = true, bool no_full = false, bool do_it = false);

    void show_fmt(const Format & fmt, const char * spaces) const {
      #if DEBUGGING
        std::cout       << spaces                  <<
          "Fmt: align:" << (int)fmt.align          <<
          " valign:"    << (int)fmt.vertical_align << 
          " Idx:"       << fmt.font_index          << 
          " Sz:"        << fmt.font_size           << 
          " St:"        << (int)fmt.font_style     << 
          " ind:"       << fmt.indent              << 
          " lhf:"       << fmt.line_height_factor  << 
          " m:"         << fmt.margin_bottom       << 
          ","           << fmt.margin_left         << 
          ","           << fmt.margin_right        << 
          ","           << fmt.margin_top          << 
          " s:"         << fmt.screen_bottom       << 
          ","           << fmt.screen_left         << 
          ","           << fmt.screen_right        << 
          ","           << fmt.screen_top          << 
          " tr:"        << fmt.trim                << 
          " pr:"        << fmt.pre                 << 
          " tt:"        << (int)fmt.text_transform <<
          " di:"        << (int)fmt.display        <<
          std::endl;
      #endif
    }

    void show_display_list(const DisplayList & list, const char * title) const;
    bool        show_cover(Image & img);
    void         put_image(Image::ImageData & image, Pos pos); 
    void     put_highlight(Dim dim, Pos pos);  
    void   clear_highlight(Dim dim, Pos pos);  
    void       put_rounded(Dim dim, Pos pos);  
    void     clear_rounded(Dim dim, Pos pos);  
    void      clear_region(Dim dim, Pos pos);
    void        set_region(Dim dim, Pos pos);

    void show_controls(const char * spaces) const {
      #if DEBUGGING
        std::cout         << spaces      <<
          " pos.x:"       << pos.x       <<
          " pos.y:"       << pos.y       <<
          " min_x:"       << min_x       <<
          " max_x:"       << max_x       <<
          " min_y:"       << min_y       <<
          " max_y:"       << max_y       <<
          " para_min_x:"  << para_min_x  <<
          " para_max_x:"  << para_max_x  <<
          " indent:"      << para_indent <<
          " width:"       << line_width  << 
          std::endl;
      #endif
    }

    inline void                 set_compute_mode(ComputeMode mode) { compute_mode = mode; }

    inline ComputeMode          get_compute_mode() const { return compute_mode;           }
    inline int16_t                   paint_width() const { return max_x - min_x;          }
    inline bool                          is_full() const { return screen_is_full;         }
    inline bool                         is_empty() const { return display_list.empty();   }
    inline bool                some_data_waiting() const { return !line_list.empty();     }
    inline const DisplayList &  get_display_list() const { return display_list;           }
    inline const DisplayList &     get_line_list() const { return line_list;              }
    inline int16_t                     get_pos_y() const { return pos.y;                  }

    int16_t       get_pixel_value(const CSS::Value & value, const Format & fmt, int16_t ref, bool vertical = false);
    int16_t       get_point_value(const CSS::Value & value, const Format & fmt, int16_t ref);
    float        get_factor_value(const CSS::Value & value, const Format & fmt, float ref);
    void            adjust_format(DOM::Node * dom_current_node, Format & fmt, CSS * element_css, CSS * item_css);
    void adjust_format_from_rules(Format & fmt, const CSS::RulesMap & rules);

    inline void reset_font_index(Format & fmt, Fonts::FaceStyle style) {
      if (style != fmt.font_style) {
        int16_t idx = -1;
        if ((idx = fonts.get_index(fonts.get_name(fmt.font_index), style)) == -1) {
          // LOG_E("Font not found 2: %s %d", fonts.get_name(fmt.font_index), style);
          idx = fonts.get_index("Default", style);
        }
        if (idx == -1) {
          fmt.font_style = Fonts::FaceStyle::NORMAL;
          fmt.font_index = 1;
        }
        else {
          fmt.font_style = style;
          fmt.font_index = idx;
        }
      }
    }
};

#if __PAGE__
  Page page;
#else
  extern Page page;
#endif
