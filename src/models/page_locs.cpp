#define __PAGE_LOCS__ 1
#include "models/page_locs.hpp"

#include "viewers/book_viewer.hpp"

bool 
PageLocs::retrieve_asap(int16_t itemref_index) {
  book_viewer.build_page_locs();
  computation_completed();
  show();
  return true;
}

const PageLocs::PageId * 
PageLocs::get_next_page_id(const PageId & page_id, int16_t count) 
{
  PagesMap::iterator it = check_and_find(page_id);
  if (it == pages_map.end()) return &check_and_find(PageId(0,0))->first;
  PageId id = page_id;

  for (int16_t cptr = count; cptr > 0; cptr--) {
    PagesMap::iterator prev = it;
    id.offset += it->second.size;
    it = pages_map.find(id);
    if (it == pages_map.end()) {
      id.itemref_index += 1; id.offset = 0;
      it = check_and_find(id);
      if (it == pages_map.end()) {
        if (count > 1) return &prev->first;
        else return &check_and_find(PageId(0,0))->first;
      }
    }
  }
  return &it->first;
}

const PageLocs::PageId * 
PageLocs::get_prev_page_id(const PageId & page_id, int count) 
{
  PagesMap::iterator it = check_and_find(page_id);
  if (it == pages_map.end()) return &check_and_find(PageId(0,0))->first;
  PageId id = page_id;
    
  for (int16_t cptr = count; cptr > 0; cptr--) {
    if (id.offset == 0) {
      if (id.itemref_index == 0) {
        if (count == 1) id.itemref_index = item_count - 1;
        else return &it->first;
      }
      else id.itemref_index--;
      if (items_set.find(id.itemref_index) == items_set.end()) retrieve_asap(id.itemref_index);
    }
    
    if (it == pages_map.begin()) it = pages_map.end();
    it--;
    id = it->first;
  }
  return &it->first;
}

const PageLocs::PageId * 
PageLocs::get_page_id(const PageId & page_id) 
{
  PagesMap::iterator it   = check_and_find(PageId(page_id.itemref_index, 0));
  PagesMap::iterator res  = pages_map.end();
  while ((it != pages_map.end()) && (it->first.itemref_index == page_id.itemref_index)) {
    if ((it->first.offset <= page_id.offset) && ((it->first.offset + it->second.size) > page_id.offset)) { res = it; break; }
    it++;
  }
  return (res == pages_map.end()) ? nullptr : &res->first ;
}

