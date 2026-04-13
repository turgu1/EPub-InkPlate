// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include <map>
#include <mutex>
#include <set>
#include <thread>

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
#include "models/page_locs_state.hpp"
#include "viewers/html_interpreter.hpp"
#include "viewers/page.hpp"

#include "pugixml.hpp"

/**
 * class PageLocs - Compute pages locations
 *
 * This class is used to compute every page locations for an ebook. This is
 * required to get fast retrieval of a page when required by the user. Page
 * locations are saved on disk once computed. Any change of font, font size,
 * screen orientation (portrait <-> landscape) will trigger a recomputation.
 */

class PageLocs {
public:
  struct PageInfo {
    int32_t size;
    int16_t page_number;
    PageInfo(int32_t siz, int16_t pg_nbr) {
      size        = siz;
      page_number = pg_nbr;
    }
    PageInfo() {};
  };

  struct PageCompare {
    bool operator()(const PageId &lhs, const PageId &rhs) const {
      if (lhs.itemref_index < rhs.itemref_index) return true;
      if (lhs.itemref_index > rhs.itemref_index) return false;
      return lhs.offset < rhs.offset;
    }
  };

  using PagePair = std::pair<const PageId, PageInfo>;
  using PagesMap = std::map<PageId, PageInfo, PageCompare>;
  using ItemsSet = std::set<int16_t>;

  enum class Req : int8_t {
    NONE, ASAP_READY, STOPPED, PERCENT
  };

  struct QueueData {
    Req req{Req::NONE};
    int16_t itemref_index{0};
  };

  static inline auto send(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      return mq_send(mgr_queue, (const char *)&data, sizeof(data), 1);
    #else
      return xQueueSendToBack(mgr_queue, &data, timeout);
    #endif
  }

private:
  static constexpr const char *TAG                = "PageLocs";
  static constexpr const int8_t LOCS_FILE_VERSION = 3;

  bool completed;
  int16_t page_count;

  std::recursive_timed_mutex mutex;

  PageLocsStatePtr state_task;

  PagesMap pages_map;
  ItemsSet items_set;
  int16_t item_count;

  std::string current_filename;

  void show();
  bool retrieve_asap(int16_t itemref_index);
  PagesMap::iterator check_and_find(const PageId &page_id);

  // ----- Page Locations computation -----

  EPub::ItemInfo item_info;
  EPub::BookFormatParams current_format_params;

  // int32_t           current_offset;          ///< Where we are in current item
  // int32_t           start_of_page_offset;

  // bool              start_of_paragraph;  ///< Required to manage paragraph indentation at
  // beginning of new page.

  // bool           page_end(Page::Format & fmt);
  // bool  page_locs_recurse(pugi::xml_node node, Page::Format fmt, DOM::Node * dom_node);

  bool load(const std::string &epub_filename); ///< load pages location from .locs file
  bool save(const std::string &epub_filename); ///< save pages location to .locs file

  #if EPUB_LINUX_BUILD
    static mqd_t mgr_queue;
    static constexpr mq_attr mgr_attr = {0, 5, sizeof(QueueData), 0};
  #else
    static QueueHandle_t mgr_queue;
  #endif

  auto receive(QueueData &data, int timeout = -1) {
    #if EPUB_LINUX_BUILD
      return mq_receive(mgr_queue, (char *)&data, sizeof(data), nullptr);
    #else
      return xQueueReceive(mgr_queue, &data, timeout);
    #endif
  }

  void setup_pages_computation(EPubPtr &epub);

public:
  PageLocs() : completed(false), page_count(0), item_count(0) {};

  void abort_threads();

  const PageId *get_next_page_id(const PageId &page_id, int16_t count = 1);
  const PageId *get_prev_page_id(const PageId &page_id, int count = 1);
  const PageId *get_page_id(const PageId &page_id);

  uint16_t get_current_itemref_index() { return item_info.itemref_index; }
  const EPub::ItemInfo &get_item_info() { return item_info; }
  const PagesMap &get_pages_map() { return pages_map; }

  void check_for_format_changes(EPubPtr &epub, int16_t itemref_index, bool force = false);
  void computation_completed();
  void start_new_document(EPubPtr &epub, int16_t itemref_index);
  void stop_document();

  inline const PageInfo *get_page_info(const PageId &page_id) {
    std::scoped_lock guard(mutex);
    PagesMap::iterator it = check_and_find(page_id);
    return it == pages_map.end() ? nullptr : &it->second;
  }

  bool insert(PageId &id, PageInfo &info);

  inline void clear() {
    std::scoped_lock guard(mutex);
    pages_map.clear();
    items_set.clear();
    completed = false;
  }

  int16_t get_page_count();

  inline int16_t get_page_nbr(const PageId &id) {
    std::scoped_lock guard(mutex);
    if (!completed) {
      LOG_W("Page Locs not completed.");
      return -1;
    }
    const PageInfo *info = get_page_info(id);
    return info == nullptr ? -1 : info->page_number;
  };
};

#if __PAGE_LOCS__
  PageLocs page_locs;
#else
  extern PageLocs page_locs;
#endif