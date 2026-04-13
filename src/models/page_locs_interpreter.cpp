#include "page_locs_interpreter.hpp"

#include "page_locs.hpp"

bool PageLocsInterpreter::page_end(const Page::Format &fmt) {

  bool res = true;
  // if ((item_info.itemref_index == 0) || !page_out->is_empty()) {

  PageId page_id = PageId(item_info.itemref_index, start_offset);

  PageLocs::PageInfo page_info = PageLocs::PageInfo(current_offset - start_offset, -1);

  if ((page_info.size > 0) || ((page_id.itemref_index == 0) && (page_id.offset == 0))) {
    if (page_info.size == 0) {
      // Patch for the case when it's the title page and no picture is to be shown
      page_info.size = 1;
    }
    if ((item_info.itemref_index > 0) && (page->is_empty())) {
      page_info.size = -page_info.size; // The page will not be counted nor displayed
    }

    page_locs.insert(page_id, page_info);

    #if DEBUGGING
      std::cout << page_id.offset << '|' << page_id.offset + page_info.size << ", "
                << page_info.page_number << ", " << page_info.size << std::endl;
    #endif
  }

  // Gives the chance to book_viewer to show a page if required
  // book_viewer.get_mutex().unlock();
  std::this_thread::yield();
  // book_viewer.get_mutex().lock();

  // LOG_D("Page %d, offset: %d, size: %d", epub.get_page_count(), loc.offset, loc.size);

  //   #if DEBUGGING
  //     std::cout << pages_map.size() << std::endl;
  //   #endif
  //   check_page_to_show(pages_map.size()); // Debugging stuff
  //}

  start_offset = current_offset;

  page->start(fmt); // Start a new page
  // beginning_of_page = true;

  return res;
}