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
#include <map>

class EPub
{
  public:
    enum class MediaType { XML, JPEG, PNG, GIF, BMP };

    typedef std::list<CSS *> CSSList;

  private:
   static constexpr char const * TAG = "EPub";

    pugi::xml_document opf;    ///< The OPF document description.
    pugi::xml_document current_item;
    pugi::xml_node     current_itemref;

    int16_t            current_itemref_index;
    char *             opf_data;
    char *             current_item_data;
    std::string        current_filename;
    std::string        opf_base_path;
    std::string        current_item_file_path;

    CSSList            css_cache;             ///< All css files in the ebook are maintained here.
    CSSList            temp_css_cache;        ///< style attributes part of the current processed item are kept here. They will be destroyed when the item is no longer required.
    CSSList            current_item_css_list; ///< List of css sources for the current item file shown. Those are indexes inside css_cache.
    CSS *              current_item_css;      ///< Ghost CSS created through merging css suites from current_item_css_list.
 
    bool               file_is_open;

    const char *        get_meta(const std::string & name);
    bool                 get_opf(std::string & filename);
    bool          check_mimetype();
    bool        get_opf_filename(std::string & filename);
    void retrieve_fonts_from_css(CSS & css);

  public:
    EPub();
   ~EPub();

    void      retrieve_css(pugi::xml_document & item_doc,
                           CSSList &            item_css_list,
                           CSS **               item_css);
    void   clear_item_data();
    bool         open_file(const std::string & epub_filename);
    bool        close_file();
    bool         get_image(std::string & filename,
                           Page::Image & image,
                           int16_t &     channel_count);
    char*    retrieve_file(const char * fname, uint32_t& size);
    bool    get_first_item();
    bool     get_next_item();
    char *        get_item(pugi::xml_node       itemref, 
                           pugi::xml_document & item_doc);
    bool get_item_at_index(int16_t itemref_index);
    bool get_item_at_index(int16_t              itemref_index,
                           char **              item_data,
                           pugi::xml_document & item_doc,
                           CSSList &            item_css_list,
                           CSS **               item_css);
    int16_t get_item_count();

    /**
     * @brief Retrieve cover's filename
     *
     * Look inside the opf file to grab the cover filename. First search in the
     * metadata. If not found, search in the manifest for an entry with type
     * cover-image
     *
     * @return char * filename, or nullptr if not found
     */
    const char* get_cover_filename();

    inline const CSSList &                   get_css_cache() const { return css_cache;                  }
    inline CSS *                      get_current_item_css() const { return current_item_css;           }
    inline const std::string &  get_current_item_file_path() const { return current_item_file_path;     }
    inline int16_t                       get_itemref_index() const { return current_itemref_index;      }
    inline const char *                          get_title()       { return get_meta("dc:title");       }
    inline const char *                         get_author()       { return get_meta("dc:creator");     }
    inline const char *                    get_description()       { return get_meta("dc:description"); }
    inline const pugi::xml_document &     get_current_item()       { return current_item;               }
    inline const std::string &        get_current_filename()       { return current_filename;           }
  };

#if __EPUB__
  EPub epub;
#else
  extern EPub epub;
#endif

#endif