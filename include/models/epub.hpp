// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __EPUB_HPP__
#define __EPUB_HPP__

#include "global.hpp"

#include "pugixml.hpp"

#include "models/css.hpp"
#include "viewers/page.hpp"

#include <string>
#include <list>
#include <forward_list>

class EPub
{
  public:
    enum MediaType { XML, JPEG, PNG, GIF, BMP };

    typedef std::list<CSS *> CSSList;
    struct Location {
      int32_t offset;                ///< Position as a number of characters in the item.
      int32_t size;                  ///< Number of chars in page.
      int16_t itemref_index;         ///< OPF itemref index in the manifest.
    };
    typedef std::vector<Location> PageLocs;

  private:
   static constexpr char const * TAG = "EPub";

    pugi::xml_document opf;    ///< The OPF document description.
    pugi::xml_node current_itemref;
    int16_t current_itemref_index;
    char *  opf_data;
    char *  current_item_data;
    std::string opf_base_path;
    std::string current_item_file_path;

    PageLocs page_locs;              ///< Pages location list for the current document.

    CSSList   css_cache;             ///< All css files in the ebook are maintained here.
    CSSList   temp_css_cache;        ///< style attributes part of the current processed item are kept here. They will be destroyed when the item is no longer required.
    CSSList   current_item_css_list; ///< List of css sources for the current item file shown. Those are indexes inside css_cache.
    CSS *     current_item_css;      ///< Ghost CSS created through merging css suites from current_item_css_list.
 
    MediaType item_media_type;
    bool      file_is_open;

    const char * get_meta(const std::string & name);
    bool         get_opf(std::string & filename);
    bool         check_mimetype();
    bool         get_opf_filename(std::string & filename);
    void         retrieve_fonts_from_css(CSS & css);

  public:
    EPub();
   ~EPub();

    void clear_item_data();
    
    pugi::xml_document current_item;

    bool open_file(const std::string & epub_filename);
    bool close_file();

    int16_t       get_itemref_index() { return current_itemref_index;      };
    const char *  get_title()         { return get_meta("dc:title");       };
    const char *  get_author()        { return get_meta("dc:creator");     };
    const char *  get_description()   { return get_meta("dc:description"); };
    
    bool    get_image(std::string & filename, Page::Image & image, int16_t & channel_count);
  
    char *  retrieve_file(const char * fname, uint32_t & size);

    /*
     * @brief pages locations retrieval from the book list directory. 
     * 
     * This is called by the BookController to retrieve
     * the list of pages locations in a book. It calls the BooksDir class
     * that will populate the list from the database.
     */
    void retrieve_page_locs(int16_t idx) ;

    /**
     * @brief Get the page nbr from offset
     * 
     * @param offset The offset from the beginning of the book.
     * @return int16_t The page number, or 0 if offset is too large.
     */
    int16_t get_page_nbr_from_offset(int32_t offset);

    CSSList     & get_css_cache() { return css_cache; };
    CSS         * get_current_item_css() { return current_item_css; };
    std::string & get_current_item_file_path() { return current_item_file_path; };

    bool get_first_item();
    bool get_next_item();
    bool get_previous_item();
    bool get_item(pugi::xml_node itemref);
    bool get_item_at_index(int16_t itemref_index);

    /**
     * @brief Retrieve cover's filename
     * 
     * Look inside the opf file to grab the cover filename. First search in the metadata. If
     * not found, search in the manifest for an entry with type cover-image
     * 
     * @return char * filename, or nullptr if not found 
     */
    const char * get_cover_filename();
    
    /**
     * @brief Get the current ebook page count
     * 
     * @return int16_t The number of pages in the book, or 0 if no file opened.
     */
    inline int16_t  get_page_count() { return file_is_open ? page_locs.size() : 0; }
    inline void       add_page_loc(Location & loc) { page_locs.push_back(loc); }
    inline void    clear_page_locs(int16_t initial_size) { page_locs.clear(); page_locs.reserve(initial_size); }
    inline const Location & get_page_loc(int16_t page_nbr) { return page_locs[page_nbr]; }
    inline const PageLocs & get_page_locs() const { return page_locs; }
  };

#if __EPUB__
  EPub epub;
#else
  extern EPub epub;
#endif

#endif