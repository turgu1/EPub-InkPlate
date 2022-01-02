// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include <thread>
#include <mutex>
#include <map>
#include <set>

#if EPUB_LINUX_BUILD
  #include <fcntl.h>
  #include <mqueue.h>
  #include <sys/stat.h>
#else
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  // #include "freertos/semphr.h"
#endif

#include "models/epub.hpp"
#include "models/dom.hpp"
#include "viewers/page.hpp"
#include "viewers/html_interpreter.hpp"

#include "pugixml.hpp"

/**
 * class PageLocs - Compute pages locations
 * 
 * This class is used to compute every page locations for an ebook. This is
 * required to get fast retrieval of a page when required by the user. Page
 * locations are saved on disk once computed. Any change of font, font size,
 * screen orientation (portrait <-> landscape) will trigger a recomputation.
 */

class PageLocs
{
  public:
    struct PageId {
      int16_t itemref_index;
      int32_t offset;
      PageId(int16_t idx, int32_t off) {
        itemref_index = idx;
        offset = off;
      }
      PageId() {
        itemref_index = 0;
        offset = 0;
      }
    };

    struct PageInfo {
      int32_t size;
      int16_t page_number;
      PageInfo(int32_t siz, int16_t pg_nbr) {
        size = siz;
        page_number = pg_nbr;
      }
      PageInfo() {};
    };
    typedef std::pair<const PageId, PageInfo> PagePair;

  private:
    static constexpr const char * TAG               = "PageLocs";
    static constexpr const int8_t LOCS_FILE_VERSION = 3;

    Page page_out;

    bool    completed;
    int16_t page_count;

    DOM * dom;
    
    struct PageCompare {
      bool operator() (const PageId & lhs, const PageId & rhs) const { 
        if (lhs.itemref_index < rhs.itemref_index) return true;
        if (lhs.itemref_index > rhs.itemref_index) return false;
        return lhs.offset < rhs.offset; 
      }
    };
    typedef std::map<PageId, PageInfo, PageCompare> PagesMap;
    typedef std::set<int16_t> ItemsSet;

    std::recursive_timed_mutex  mutex;

    std::thread state_thread;
    std::thread retriever_thread;

    PagesMap  pages_map;
    ItemsSet  items_set;
    int16_t   item_count;

    void show();
    bool retrieve_asap(int16_t itemref_index);
    PagesMap::iterator check_and_find(const PageId & page_id);

    // ----- Page Locations computation -----
    
    EPub::ItemInfo         item_info;    
    EPub::BookFormatParams current_format_params;

    //int32_t           current_offset;          ///< Where we are in current item
    //int32_t           start_of_page_offset;
    int16_t           page_bottom;
    bool              show_images;
    //bool              start_of_paragraph;  ///< Required to manage paragraph indentation at beginning of new page.
    
    //bool           page_end(Page::Format & fmt);
    //bool  page_locs_recurse(pugi::xml_node node, Page::Format fmt, DOM::Node * dom_node);

    bool load(const std::string & epub_filename); ///< load pages location from .locs file
    bool save(const std::string & epub_filename); ///< save pages location to .locs file

  public:

    PageLocs() : 
      completed(false), 
      item_count(0),
      page_count(0)

      { };

    void setup();
    void abort_threads();
    bool build_page_locs(int16_t itemref_index);

    const PageId * get_next_page_id(const PageId & page_id, int16_t count = 1);
    const PageId * get_prev_page_id(const PageId & page_id, int     count = 1);
    const PageId *      get_page_id(const PageId & page_id                   );

    uint16_t   get_current_itemref_index() { return item_info.itemref_index; }
    const EPub::ItemInfo & get_item_info() { return item_info;               }
    const PagesMap       & get_pages_map() { return pages_map;               }

    void check_for_format_changes(int16_t count, int16_t itemref_index, bool force = false);
    void    computation_completed();
    void       start_new_document(int16_t count, int16_t itemref_index);
    void            stop_document();

    inline const PageInfo* get_page_info(const PageId & page_id) {
      std::scoped_lock   guard(mutex);
      PagesMap::iterator it = check_and_find(page_id);
      return it == pages_map.end() ? nullptr : &it->second;
    }

    bool insert(PageId & id, PageInfo & info);

    inline void clear() { 
      std::scoped_lock guard(mutex);
      pages_map.clear(); 
      items_set.clear();
      completed = false; 
    }

    inline int16_t get_page_count() { return completed ? page_count : -1; }

    inline int16_t get_page_nbr(const PageId & id) {
      std::scoped_lock guard(mutex);
      if (!completed) return -1; 
      const PageInfo * info = get_page_info(id);
      return info == nullptr ? -1 : info->page_number;
    };
};

#if __PAGE_LOCS__
  PageLocs page_locs;
#else
  extern PageLocs page_locs;
#endif