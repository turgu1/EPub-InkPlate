#ifndef __PAGE_LOCS_HPP__
#define __PAGE_LOCS_HPP__

#include "global.hpp"

#include <map>
#include <set>

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
      PageId() {}
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
    bool completed;
    struct PageCompare {
      bool operator() (const PageId & lhs, const PageId & rhs) const { 
        if (lhs.itemref_index < rhs.itemref_index) return true;
        if (lhs.itemref_index > rhs.itemref_index) return false;
        return lhs.offset < rhs.offset; 
      }
    };
    typedef std::map<PageId, PageInfo, PageCompare> PagesMap;
    typedef std::set<int16_t> ItemsSet;

    PagesMap pages_map;
    ItemsSet items_set;
    int16_t item_count;

    void show() {
      std::cout << "----- Page Locations -----" << std::endl;
      for (auto & entry : pages_map) {
        std::cout 
          << " idx: " << entry.first.itemref_index 
          << " off: " << entry.first.offset 
          << " siz: " << entry.second.size
          << " pg: "  << entry.second.page_number << std::endl;
      }
      std::cout << "----- End Page Locations -----" << std::endl;
    }

    bool retrieve_asap(int16_t itemref_index);

    PagesMap::iterator check_and_find(const PageId & page_id) {
      if (!completed && (items_set.find(page_id.itemref_index) == items_set.end())) {
        if (!retrieve_asap(page_id.itemref_index)) return pages_map.end();
      }
      return pages_map.find(page_id);
    }

  public:
    PageLocs() : completed(false), item_count(0) { };

    const PageId * get_next_page_id(const PageId & page_id, int16_t count = 1);
    const PageId * get_prev_page_id(const PageId & page_id, int     count = 1);
    const PageId *      get_page_id(const PageId & page_id                   );

    inline const PageInfo * get_page_info(const PageId & page_id) {
      PagesMap::iterator it = check_and_find(page_id);
      return it == pages_map.end() ? nullptr : &it->second;
    }

    inline void insert(PageId & id, PageInfo & info) {
      pages_map.insert(std::make_pair(id, info));
      items_set.insert(id.itemref_index);
    }

    inline void clear() { 
      pages_map.clear(); 
      items_set.clear();
      completed = false; 
    }

    inline void  size() { pages_map.size();  }

    void computation_completed() {
      if (!completed) {
        int16_t page_nbr = 0;
        for (auto & entry : pages_map) {
          entry.second.page_number = page_nbr++;
        }
        completed = true;
      }
    }

    inline int16_t page_count() { return completed ? pages_map.size() : -1; }

    inline void set_item_count(int16_t count) { item_count = count; }

    inline int16_t page_nbr(const PageId & id) {
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

#endif