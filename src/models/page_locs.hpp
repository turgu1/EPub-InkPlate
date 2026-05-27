// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#if EPUB_LINUX_BUILD
  #include <fcntl.h>
  #include <mqueue.h>
  #include <sys/stat.h>
  #include <time.h>
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

#if 0
  #define SHOW_IT(msg, ...) LOG_I(msg, ##__VA_ARGS__)
#else
  #define SHOW_IT(msg, ...)
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
  using PagesMap = HimemMap<PageId, PageInfo, PageCompare>;
  using ItemsSet = HimemSet<int16_t>;

  enum class Req : int8_t { NONE, ASAP_READY, STOPPED, PERCENT, COMPLETED };

  struct QueueData {
    Req req{Req::NONE};
    uint32_t correlationId{0}; // Hardening #1: correlation ID for request matching
    int16_t itemrefIndex{0};
  };

  static inline auto send(const QueueData data, int timeout = 0) {
#if EPUB_LINUX_BUILD
    return mq_send(mgrQueue, (const char *)&data, sizeof(data), 1);
#else
    return xQueueSendToBack(mgrQueue, &data, timeout);
#endif
  }

  [[nodiscard]] inline auto isControlTaskReadyToBeStopped() const -> bool {
    return controlTaskReadyToBeStopped;
  }

  // Telemetry for protocol deviation detection
  struct Telemetry {
    std::atomic<uint32_t> nextRequestId{1};
    std::atomic<uint64_t> asapRequestsSubmitted{0};
    std::atomic<uint64_t> asapRepliesMatched{0};
    std::atomic<uint64_t> asapRepliesMismatched{0};
    std::atomic<uint64_t> asapReplyTimeouts{0};
    std::atomic<uint64_t> percentRequestsSubmitted{0};
    std::atomic<uint64_t> mapInsertions{0};
    std::atomic<uint64_t> mapLockContentions{0};
    std::atomic<uint64_t> queueSendFailures{0};

    // Hardening #3: Bounded waits/timeouts
    std::atomic<uint64_t> stopTaskTimeouts{0};
    std::atomic<uint64_t> stopTaskRetries{0};
    std::atomic<uint64_t> stopTaskFailures{0};
  } telemetry;

  // Hardening #3: Timeout constants (in milliseconds)
  static constexpr int STOP_WAIT_TIMEOUT_MS  = 2000; // 2 second initial wait
  static constexpr int STOP_RETRY_TIMEOUT_MS = 500;  // 500ms per retry
  static constexpr int STOP_MAX_RETRIES      = 4;    // Max 4 retries = 2 seconds total
  static constexpr int STOP_TOTAL_TIMEOUT_MS = 4000; // 4 second hard limit
private:
  static constexpr const char *TAG                = "PageLocs";
  static constexpr const int8_t LOCS_FILE_VERSION = 4;

  bool completed{false};
  bool aborted{false};
  bool controlTaskReadyToBeStopped{false};
  bool pendingSave{false};
  std::atomic<bool> computationDone{false};

  int16_t pageCount{0};

  std::recursive_timed_mutex mutex;

  PageLocsControlPtr controlTask{nullptr};

  PagesMap pagesMap;
  ItemsSet itemsSet;
  int16_t itemCount{0};
  std::atomic<uint32_t> generatedPageEntryCount{0};

  std::string currentFilename;

  // Hardening #1: Track pending requests with correlation IDs
  struct PendingRequest {
    uint32_t correlationId;
    enum Type { ASAP, PERCENT, STOP } type;
    std::condition_variable cv;
    bool completed{false};
    int16_t result{-1};
  };

  auto show() -> void;
  auto retrieveAsap(int16_t itemrefIndex) -> bool;
  auto checkAndFind(const PageId &pageId) -> PagesMap::iterator;

  // ----- Page Locations computation -----

  EPub::ItemInfo itemInfo;
  EPub::BookFormatParams currentFormatParams;

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
    if (timeout < 0) {
      return mq_receive(mgrQueue, (char *)&data, sizeof(data), nullptr);
    }

    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    const int64_t nsecTotal =
        static_cast<int64_t>(ts.tv_nsec) + static_cast<int64_t>(timeout) * 1000000LL;
    ts.tv_sec += static_cast<time_t>(nsecTotal / 1000000000LL);
    ts.tv_nsec = static_cast<long>(nsecTotal % 1000000000LL);
    return mq_timedreceive(mgrQueue, (char *)&data, sizeof(data), nullptr, &ts);
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

  auto getItemInfo() -> const EPub::ItemInfo & { return itemInfo; }
  auto getPagesMap() -> const PagesMap & { return pagesMap; }

  auto checkForFormatChanges(EPubPtr &epub, int16_t itemrefIndex, bool force = false) -> void;
  auto computationCompleted() -> void;
  auto computationAborted(std::string reason) -> void;
  auto startNewDocument(EPubPtr &epub, int16_t itemrefIndex) -> void;

  [[nodiscard]] inline auto isRunning() const -> bool { return controlTask != nullptr; }

  [[nodiscard]] inline auto getPageInfo(const PageId &pageId) -> const PageInfo * {
    if (isControlTaskReadyToBeStopped()) stopControlTask();

    std::scoped_lock guard(mutex);
    PagesMap::iterator it = checkAndFind(pageId);
    return it == pagesMap.end() ? nullptr : &it->second;
  }

  [[nodiscard]] inline auto getCurrentItemrefIndex() const {
    return (controlTask) ? controlTask->getCurrentItemrefIndex() : -1;
  }

  [[nodiscard]] inline auto getGeneratedPageEntryCount() const -> uint32_t {
    return generatedPageEntryCount.load();
  }

  [[nodiscard]] inline auto isComputationCompleted() const -> bool {
    return computationDone.load();
  }

  auto insert(PageId &id, PageInfo &info) -> void;

  inline auto clear() -> void {
    if (isControlTaskReadyToBeStopped()) stopControlTask();

    {
      std::scoped_lock guard(mutex);
      pagesMap.clear();
      itemsSet.clear();
      generatedPageEntryCount.store(0);
      completed                   = false;
      aborted                     = false;
      controlTaskReadyToBeStopped = false;
      pendingSave                 = false;
      computationDone.store(false);
    }
  }

  auto getPageCountOrPercent() -> int16_t;

  [[nodiscard]] inline auto getPageNbr(const PageId &id) -> int16_t {
    if (isControlTaskReadyToBeStopped()) stopControlTask();

    {
      std::scoped_lock guard(mutex);
      if (!completed) {
        // LOG_W("Page Locs not completed.");
        return -1;
      }
      PagesMap::iterator it = checkAndFind(id);
      return it == pagesMap.end() ? -1 : it->second.pageNumber;
    }
  };
};

#if __PAGE_LOCS__
PageLocs pageLocs;
#else
extern PageLocs pageLocs;
#endif