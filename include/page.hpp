#ifndef __PAGE_HPP__
#define __PAGE_HPP__

#include <string>
#include <list>

#include "global.hpp"
#include "fonts.hpp"
#include "css.hpp"

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
      int16_t            font_size;     ///< In pixels
      int16_t            indent;        ///< In pixels
      int16_t            margin_left;   ///< In pixels
      int16_t            margin_right;  ///< In pixels
      int16_t            margin_top;    ///< In pixels
      int16_t            margin_bottom; ///< In pixels
      int16_t            screen_left;   ///< In pixels
      int16_t            screen_right;  ///< In pixels
      int16_t            screen_top;    ///< In pixels
      int16_t            screen_bottom; ///< In pixels
      int16_t            width;         ///< In pixels
      int16_t            height;        ///< In pixels
      bool trim;
      Fonts::FaceStyle   font_style;
      CSS::Align         align;
      CSS::TextTransform text_transform;
    };

    struct Image {
      const uint8_t * bitmap;
      int16_t width;
      int16_t height;
    };

    enum ComputeMode { LOCATION, MOVE, DISPLAY };

  private:
    static constexpr char const * TAG = "Page";

    enum DisplayListCommand { GLYPH, IMAGE, HIGHLIGHT_REGION, CLEAR_REGION };
    struct DisplayListEntry {
      union Kind {
        struct GryphEntry {            ///< Used for GLYPH
          TTF::BitmapGlyph * glyph;    ///< Glyph
        } glyph_entry;
        struct ImageEntry {           ///< Used for IMAGE
          Image image;       
          int16_t advance;             ///< Horizontal advance on the baseline
        } image_entry;
        struct RegionEntry {           ///< Used for HIGHLIGHT_REGION and CLEAR_REGION
          int16_t width, height;       ///< Region dimensions
        } region_entry;
      } kind;
      int16_t x, y;                    ///< Screen coordinates
      DisplayListCommand command;      ///< Command
    };

    typedef std::list<DisplayListEntry> DisplayList;

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

    DisplayList display_list; ///< The list of characters and their position to put on screen
    DisplayList line_list;    ///< Line preparation for paragraphs

    bool screen_is_full;                 ///< True if screen no more space to add characters

    int16_t xpos, ypos;                  ///< Current drawing Screen position
    int16_t min_y, max_x, max_y, min_x;  ///< Screen limits for page content
    int16_t para_max_x, para_min_x;

    int16_t line_width,  glyphs_height;
    int16_t para_indent, top_margin;

    void clear_display_list();
    void clear_line_list();

    void add_line(const Format & fmt, bool justifyable);
    void add_glyph_to_line(TTF::BitmapGlyph * glyph, TTF & font, bool is_space);
    void add_image_to_line(Image & image, int16_t advance, bool copy);

  public:

    Page();
   ~Page();

    void set_compute_mode(ComputeMode mode) { compute_mode = mode; }
    ComputeMode get_compute_mode() { return compute_mode; }

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
     * @brief End the current paragraph
     * 
     * @param fmt Formatting parameters.
     */
    bool end_paragraph(const Format & fmt);

    /**
     * @brief Line Break
     * 
     * A line break at the end of a page when there is no
     * additional space will be ignored.
     * 
     * @param fmt Formatting parameters.
     * @return true The line break has been added to the paragraph.
     * @return false There is not enough space for a line break on page
     */
    bool line_break(const Format & fmt);

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
     * @param bitmap Image bitmap data. Each pixel is a grayscaled byte.
     * @param width  Width in bytes of the image bitmap.
     * @param height Height in bytes of the image bitmap.
     * @param fmt Formatting parameters.
     * @return true The image has been added to the paragraph
     * @return false There is not enough room to add the image.
     */
    bool add_image(Image & image, const Format & fmt);

    /**
     * @brief Put text on page
     * 
     * Simple algorithm to put text on a page at current location.
     * This is used by the books_dir_view class. It is expected that
     * the text will fit inside the page.
     * 
     * @param str Text to show
     * @param fmt Formatting parameters.
     */
    void put_text(std::string str, const Format & fmt);

    /**
     * @brief Put string to the screen.
     * 
     * This method put the string to a specific screen location.
     * 
     * @param str 
     * @param xpos If == -1, use screen margin positions
     * @param ypos 
     * @param fmt Formatting parameters.
     */
    void put_str_at(std::string & str, int16_t xpos, int16_t ypos, const Format & fmt);

    /**
     * @brief Paint the display list to the screen.
     * 
     * The screen is first erased and the painting process is done using 
     * the content of the display list.
     */
    void paint();

    int16_t paint_width() { return max_x - min_x; };

    void show_fmt(const Format & fmt, const char * spaces) const {
      std::cout       << spaces                 <<
        "Fmt: align:" << fmt.align              << 
        " fntIdx:"    << fmt.font_index         << 
        " fntSz:"     << fmt.font_size          << 
        " fntSt:"     << fmt.font_style         << 
        " ind:"       << fmt.indent             << 
        " lhf:"       << fmt.line_height_factor << 
        " mb:"        << fmt.margin_bottom      << 
        " ml:"        << fmt.margin_left        << 
        " mr:"        << fmt.margin_right       << 
        " mt:"        << fmt.margin_top         << 
        " sb:"        << fmt.screen_bottom      << 
        " sl:"        << fmt.screen_left        << 
        " sr:"        << fmt.screen_right       << 
        " st:"        << fmt.screen_top         << 
        " tr:"        << fmt.trim               << 
        std::endl;
    }

    bool show_cover(unsigned char * data, int32_t size);
    void put_image(Image & image, int16_t x, int16_t y); 
    void put_highlight(
            int16_t width, int16_t height, 
            int16_t x, int16_t y);  
    void clear_region(int16_t width, int16_t height, 
                      int16_t x, int16_t y);

    inline bool is_full() { return screen_is_full; }
    void show_controls(const char * spaces) const {
      std::cout         << spaces      <<
        " xpos:"        << xpos        <<
        " ypos:"        << ypos        <<
        " min_x:"       << min_x       <<
        " max_x:"       << max_x       <<
        " min_y:"       << min_y       <<
        " max_y:"       << max_y       <<
        " para_min_x:"  << para_min_x  <<
        " para_max_x:"  << para_max_x  <<
        " para_indent:" << para_indent <<
        " line_width:"  << line_width  << 
        std::endl;
    }

    inline bool some_data_waiting() const { return !line_list.empty(); }
};

#if __PAGE__
  Page page;
#else
  extern Page page;
#endif

#endif