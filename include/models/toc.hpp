// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

// Table of content class

#include "global.hpp"
#include "logging.hpp"
#include "alloc.hpp"
#include "pugixml.hpp"

#include "models/page_locs.hpp"
#include "helpers/char_pool.hpp"
#include "helpers/simple_db.hpp"

#include <forward_list>
#include <map>
#include <utility>

class TOC
{
  public:
    TOC() : 
             char_pool(nullptr), 
           char_buffer(nullptr), 
      char_buffer_size(0),
               ncx_opf(nullptr),
              ncx_data(nullptr),
                 ready(false),
             compacted(false),
                 saved(false),
              some_ids(false) {}
   ~TOC() { 
      if (char_pool   != nullptr) delete char_pool;
      if (char_buffer != nullptr) free(char_buffer); 
    };

    #pragma pack(push, 1)
    struct EntryRecord {
      char             * label;
      PageLocs::PageId   page_id;
      uint8_t            level;
      EntryRecord() {
        label = nullptr;
        level = 0;
      }
    };

    struct VersionRecord {
      uint16_t version;
      char     app_name[32];
      VersionRecord() {
        version = 0;
        memset(app_name, 0, 32);
      }
    };
    #pragma pack(pop)

    typedef std::map<std::pair<int16_t, std::string>, uint16_t> Infos;
    typedef std::vector<EntryRecord> Entries;

    const Entries &          get_entries()            { return entries;         }
    inline bool                 is_ready()            { return ready;           }
    inline bool                 is_empty()            { return entries.empty(); }
    inline bool        there_is_some_ids()            { return some_ids;        }
    inline int16_t       get_entry_count()            { return entries.size();  }
    inline const EntryRecord & get_entry(int16_t idx) { return entries[idx];    }
    
    /**
     * @brief Load table of content from a file.
     * 
     * This is used to load the table of content related to the current
     * ePub book. The file is a complete table with labels and page_ids. The filename is
     * the same as for the e-book, with extension .toc
     * 
     * @return True if loading the file was successfull
     */
    bool load();

    /**
     * @brief Save constructed table of content to a file.
     * 
     * This method will save the table of content once it has been
     * constructed by the PageLocs class. The filename is
     * the same as for the e-book, with extension .toc
     * 
     * @return True if saving was successfull.
     */
    bool save();

    /**
     * @brief Load table of content from ePub file.
     * 
     * This is used to prepare the construction of a table of
     * content. This method will load the  *.ncx* file, retrieving
     * ids, filenames and labels for each entry in the table of content.
     * The PageLocs class will then interact with the TOC class to complete
     * the information with the PageId related to each entry,
     * 
     * @return True if the information has been retrieved successfully.
     */
    bool load_from_epub();

    /**
     * @brief Compact the char strings to a single buffer.
     * 
     * This is used to retrieve all char strings in a single buffer
     * in preparation to be saved to a file.
     * 
     * @return True if the compaction was successfull.
     */
    bool compact();

    /**
     * @brief set table of content entry with page id
     * 
     * If the id correspond to an entry of the table
     * of content, its location will be set with the
     * page_id received.
     * 
     * @param id HTML id attribute that is part of an item.
     * @param current_offset The location offset of the id in the item
     */
    void set(std::string & id, int32_t current_offset);
    void set(int32_t current_offset);
    
  private:
    static constexpr char const * TAG            = "TOC";
    static constexpr char const * TOC_NAME       = "EPUB_INKPLATE_TOC";
    static const uint16_t         TOC_DB_VERSION = 1;

    Entries    entries;
    Infos      infos;

    SimpleDB   db;                       ///< The SimpleDB table

    CharPool * char_pool;
    char     * char_buffer;
    uint16_t   char_buffer_size;

    pugi::xml_document * ncx_opf;
    char               * ncx_data;

    const pugi::xml_document * opf;

    volatile bool ready; // true if the table of content has been populated
    bool compacted;
    bool saved;
    bool some_ids;

    void        clean();
    void        clean_filename(char * fname);
    std::string build_filename();
    bool        do_nav_points(pugi::xml_node & node, uint8_t level);

    #if DEBUGGING
      void      show();
      void      show_info();
    #endif
};

#if __TOC__
  TOC toc;
#else
  extern TOC toc;
#endif