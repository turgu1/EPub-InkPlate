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
#include "models/page_locs_control.hpp"
#include "viewers/html_interpreter.hpp"
#include "viewers/page.hpp"

#include "pugixml.hpp"

#if 1
  #define SHOW_IT(msg, ...) LOG_I(msg, ##__VA_ARGS__)
#endif

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
    int16_t pageNumber;
    PageInfo(int32_t siz, int16_t pageNbr) {
      size       = siz;
      pageNumber = pageNbr;
    }
    PageInfo() {};
  };

  struct PageCompare {
    auto operator()(const PageId &lhs, const PageId &rhs) const -> bool {
      if (lhs.itemrefIndex < rhs.itemrefIndex) return true;
      if (lhs.itemrefIndex > rhs.itemrefIndex) return false;
      return lhs.offset < rhs.offset;
    }
  };

  using PagePair = std::pair<const PageId, PageInfo>;
  using PagesMap = std::map<PageId, PageInfo, PageCompare>;
  using ItemsSet = std::set<int16_t>;

  enum class Req : int8_t {
    NONE, ASAP_READY, STOPPED, PERCENT, COMPLETED
  };

  struct QueueData {
    Req req{Req::NONE};
    int16_t itemrefIndex{0};
  };

  static inline auto send(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      return mq_send(mgrQueue, (const char *)&data, sizeof(data), 1);
    #else
      return xQueueSendToBack(mgrQueue, &data, timeout);
    #endif
  }

private:
  static constexpr const char *TAG                = "PageLocs";
  static constexpr const int8_t LOCS_FILE_VERSION = 3;

  bool completed{false};
  bool aborted{false};
  bool controlTaskReadyToBeStopped{false};

  int16_t pageCount{0};

  std::recursive_timed_mutex mutex;

  PageLocsControlPtr controlTask;

  PagesMap pagesMap;
  ItemsSet itemsSet;
  int16_t itemCount{0};

  std::string currentFilename;

  auto show() -> void;
  auto retrieveAsap(int16_t itemrefIndex) -> bool;
  auto checkAndFind(const PageId &pageId) -> PagesMap::iterator;

  // ----- Page Locations computation -----

  EPub::ItemInfo itemInfo;
  EPub::BookFormatParams currentFormatParams;

  // int32_t           currentOffset;          ///< Where we are in current item
  // int32_t           start_of_page_offset;

  // bool              start_of_paragraph;  ///< Required to manage paragraph indentation at
  // beginning of new page.

  // bool           pageEnd(Page::Format & fmt);
  // bool  page_locs_recurse(pugi::xml_node node, Page::Format fmt, DOM::Node * dom_node);

  auto load(const std::string &epubFilename) -> bool; ///< load pages location from .locs file
  auto save(const std::string &epubFilename) -> bool; ///< save pages location to .locs file

  #if EPUB_LINUX_BUILD
    static mqd_t mgrQueue;
    static constexpr mq_attr mgrAttr = {0, 5, sizeof(QueueData), 0};
  #else
    static QueueHandle_t mgrQueue;
  #endif

  auto receive(QueueData &data, int timeout = -1) {
    #if EPUB_LINUX_BUILD
      return mq_receive(mgrQueue, (char *)&data, sizeof(data), nullptr);
    #else
      return xQueueReceive(mgrQueue, &data, timeout);
    #endif
  }

  auto setupPagesComputation(EPubPtr &epub) -> void;

public:
  PageLocs() = default;

  auto stopControlTask() -> void;

  auto getNextPageId(const PageId &pageId, int16_t count = 1) -> const PageId *;
  auto getPrevPageId(const PageId &pageId, int count = 1) -> const PageId *;
  auto getPageId(const PageId &pageId) -> const PageId *;

  auto getCurrentItemrefIndex() -> uint16_t { return itemInfo.itemrefIndex; }
  auto getItemInfo() -> const EPub::ItemInfo & { return itemInfo; }
  auto getPagesMap() -> const PagesMap & { return pagesMap; }

  auto checkForFormatChanges(EPubPtr &epub, int16_t itemrefIndex, bool force = false) -> void;
  auto computationCompleted() -> void;
  auto computationAborted(std::string reason) -> void;
  auto startNewDocument(EPubPtr &epub, int16_t itemrefIndex) -> void;

  [[nodiscard]] inline auto getPageInfo(const PageId &pageId) -> const PageInfo * {
    if (controlTaskReadyToBeStopped) stopControlTask();

    std::scoped_lock guard(mutex);
    PagesMap::iterator it = checkAndFind(pageId);
    return it == pagesMap.end() ? nullptr : &it->second;
  }

  auto insert(PageId &id, PageInfo &info) -> bool;

  inline auto clear() -> void {
    if (controlTaskReadyToBeStopped) stopControlTask();

    {
      std::scoped_lock guard(mutex);
      pagesMap.clear();
      itemsSet.clear();
      completed                   = false;
      aborted                     = false;
      controlTaskReadyToBeStopped = false;
    }
  }

  auto getPageCountOrPercent() -> int16_t;

  [[nodiscard]] inline auto getPageNbr(const PageId &id) -> int16_t {
    if (controlTaskReadyToBeStopped) stopControlTask();

    {
      std::scoped_lock guard(mutex);
      if (!completed) {
        // LOG_W("Page Locs not completed.");
        return -1;
      }
      const PageInfo *info = getPageInfo(id);
      return info == nullptr ? -1 : info->pageNumber;
    }
  };
};

#if __PAGE_LOCS__
  PageLocs pageLocs;
#else
  extern PageLocs pageLocs;
#endif