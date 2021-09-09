// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "pugixml.hpp"

#include "models/css.hpp"
#include "models/book_params.hpp"
#include "viewers/page.hpp"
#include "models/image.hpp"

#include <list>
#include <forward_list>
#include <map>
#include <mutex>

class EPub
{
  public:
    enum class MediaType { XML, JPEG, PNG, GIF, BMP };
    typedef std::list<CSS *> CSSList;
    struct ItemInfo {
      std::string        file_path;
      int16_t            itemref_index;
      pugi::xml_document xml_doc;
      CSSList            css_cache;   ///< style attributes part of the current processed item are kept here. They will be destroyed when the item is no longer required.
      CSSList            css_list;    ///< List of css sources for the current item file shown. Those are indexes inside css_cache.
      CSS *              css;         ///< Ghost CSS created through merging css suites from css_list and css_cache.
      char *             data;
      MediaType          media_type;
    };

    // This struct contains the current parameters that influence
    // the rendering of e-book pages. Its content is constructed from
    // both the e-book's specific parameters and default configuration options.
    #pragma pack(push, 1)
    struct BookFormatParams {
      int8_t ident;        ///< Device identity (screen.hpp IDENT constant)
      int8_t orientation;  ///< Config option only
      int8_t show_title;
      int8_t show_images;      
      int8_t font_size;        
      int8_t use_fonts_in_book;
      int8_t font;             
    };
    #pragma pack(pop)
    
  private:
   static constexpr char const * TAG = "EPub";

    std::recursive_timed_mutex mutex;

    pugi::xml_document opf;    ///< The OPF document description.
    pugi::xml_node     current_itemref;

    char *             opf_data;
    std::string        current_filename;
    std::string        opf_base_path;

    ItemInfo           current_item_info;
    BookParams       * book_params;
    BookFormatParams   book_format_params;

    CSSList            css_cache;             ///< All css files in the ebook are maintained here.
  
    bool               file_is_open;

    const char *        get_meta(const std::string & name    );
    bool                 get_opf(std::string &       filename);
    bool          check_mimetype();
    bool        get_opf_filename(std::string &       filename);
    void retrieve_fonts_from_css(CSS &               css     );

  public:
    EPub();
   ~EPub();

    void              retrieve_css(ItemInfo &           item);
    void                load_fonts();
    void           clear_item_data(ItemInfo &           item);
    void               open_params(const std::string &  epub_filename);
    bool                 open_file(const std::string &  epub_filename);
    bool                close_file();
    Image *              get_image(std::string &        fname,
                                   bool                 load);
    char*            retrieve_file(const char *         fname, 
                                   uint32_t &           size);
    bool                  get_item(pugi::xml_node       itemref, 
                                   ItemInfo &           item);
    bool         get_item_at_index(int16_t              itemref_index);
    bool         get_item_at_index(int16_t              itemref_index,
                                   ItemInfo &           item);
    std::string    filename_locate(const char *         fname);
    int16_t         get_item_count();
    void update_book_format_params();

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

    inline const CSSList &                   get_css_cache() const { return css_cache;                       }
    inline CSS *                      get_current_item_css() const { return current_item_info.css;           }
    inline const ItemInfo &          get_current_item_info() const { return current_item_info; }
    inline const std::string &  get_current_item_file_path() const { return current_item_info.file_path;     }
    inline int16_t                       get_itemref_index() const { return current_item_info.itemref_index; }
    inline const char *                          get_title()       { return get_meta("dc:title");            }
    inline const char *                         get_author()       { return get_meta("dc:creator");          }
    inline const char *                    get_description()       { return get_meta("dc:description");      }
    inline const pugi::xml_document &     get_current_item()       { return current_item_info.xml_doc;       }
    inline const std::string &        get_current_filename()       { return current_filename;                }
    inline BookParams *                    get_book_params()       { return book_params;                     }
    inline BookFormatParams *       get_book_format_params()       { return &book_format_params;             }
    inline const std::string &           get_opf_base_path()       { return opf_base_path;                   }
    inline const pugi::xml_document &              get_opf()       { return opf;                             }
  };

#if __EPUB__
  EPub epub;
#else
  extern EPub epub;
#endif
