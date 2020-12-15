// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __BOOK_VIEWER_HPP__
#define __BOOK_VIEWER_HPP__

#include <vector>
#include <string>
#include <unordered_map>

#include "global.hpp"
#include "pugixml.hpp"
#include "viewers/page.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "models/css.hpp"
#include "models/fonts.hpp"

using namespace pugi;

class BookViewer
{
  private:
    static constexpr char const * TAG = "BookViewer";

    int32_t current_offset;          ///< Where we are in current item
    int32_t start_of_page_offset;
    int32_t end_of_page_offset;
    int16_t page_bottom;
    bool    show_images;
    PageLocs::PageId current_page_id;
    
    CSS::Properties * last_props;

    enum Element { BODY, P, LI, BR, H1, H2, H3, H4, H5, H6, 
                   B, I, A, IMG, IMAGE, EM, DIV, SPAN, PRE,
                   BLOCKQUOTE };
    
    typedef std::unordered_map<std::string, Element> Elements;
    Elements elements;
    bool start_of_paragraph;  ///< Required to manage paragraph indentation at beginning of new page.
    bool indent_paragraph;

    bool get_image(std::string & filename, Page::Image & image);
    void adjust_format(pugi::xml_node node, Page::Format & fmt, CSS::Properties * element_properties);
    void adjust_format_from_suite(Page::Format & fmt, const CSS::PropertySuite & suite);
    bool page_locs_recurse(pugi::xml_node node, Page::Format fmt);
    void page_locs_end_page(Page::Format & fmt);
    bool build_page_recurse(pugi::xml_node node, Page::Format fmt);
    void build_page_at(const PageLocs::PageId & page_id);
    int16_t get_pixel_value(const CSS::Value & value, const Page::Format & fmt, int16_t ref);
    int16_t get_point_value(const CSS::Value & value, const Page::Format & fmt, int16_t ref);
    float get_factor_value(const CSS::Value & value, const Page::Format & fmt, float ref);

    inline void reset_font_index(Page::Format & fmt, Fonts::FaceStyle style) {
      if (style != fmt.font_style) {
        int16_t idx = -1;
        if ((idx = fonts.get_index(fonts.get_name(fmt.font_index), style)) == -1) {
          // LOG_E("Font not found 2: %s %d", fonts.get_name(fmt.font_index), style);
          idx = fonts.get_index("Default", style);
        }
        if (idx == -1) {
          fmt.font_style = Fonts::NORMAL;
          fmt.font_index = 1;
        }
        else {
          fmt.font_style = style;
          fmt.font_index = idx;
        }
      }
    };
  public:

    BookViewer() :
      elements{{"p",     P}, {"div",     DIV}, {"span", SPAN}, {"br",     BR}, {"h1",                 H1},  
               {"h2",   H2}, {"h3",       H3}, {"h4",     H4}, {"h5",     H5}, {"h6",                 H6}, 
               {"b",     B}, {"i",         I}, {"em",     EM}, {"body", BODY}, {"a",                   A},
               {"img", IMG}, {"image", IMAGE}, {"li",     LI}, { "pre",  PRE}, {"blockquote", BLOCKQUOTE}}
      {  }

    ~BookViewer() { }

    void init() { current_page_id = PageLocs::PageId(-1, -1); }

    /**
     * @brief Build the pages location vector
     * 
     * The vector is used to quicly direct page preparation and display. It is expected to
     * be called once a book locations refresh is required by the BooksDir class. The information
     * is put in the ebooks list database and retrieved when the user select a book
     * to read. The process of building the list is a long process, better be done
     * only once per book at application load time.
     * 
     * ToDo: Make it runs as a thread.
     * 
     */
    bool build_page_locs();

    /**
     * @brief Show a page on the display.
     * 
     * @param page_nbr The page number to show (First ebook page = 0, cover = -1)
     */
    void show_page(const PageLocs::PageId & page_id);

    void line_added_at(int16_t ypos) {
      LOG_D("Line added: %d %d", current_offset, ypos);
    }
};

#if __BOOK_VIEWER__
  BookViewer book_viewer;
#else
  extern BookViewer book_viewer;
#endif

#endif