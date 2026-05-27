// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PAGE_LOCS__ 1
#include "models/page_locs.hpp"

#include "config.hpp"
#include "controllers/event_mgr.hpp"
#include "helpers/show_load_icon.hpp"
#include "models/page_locs_control.hpp"

#include "viewers/book_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/page.hpp"

#if EPUB_LINUX_BUILD
  #include <chrono>
#else
  #include <esp_pthread.h>
#endif

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>

#if EPUB_INKPLATE_BUILD
QueueHandle_t PageLocs::mgrQueue{nullptr};
#else
  #include <mqueue.h>
mqd_t PageLocs::mgrQueue{-1};
#endif

/**
 * Initialize manager-side computation state and queues for a new document run.
 */
auto PageLocs::setupPagesComputation(EPubPtr &epub) -> void {

  completed                   = false;
  aborted                     = false;
  controlTaskReadyToBeStopped = false;
  computationDone.store(false);

  controlTask = PageLocsControl::Make();
  controlTask->setup(epub->getCurrentFilename());

#if EPUB_LINUX_BUILD
  mq_unlink("/mgr");

  mgrQueue = mq_open("/mgr", O_RDWR | O_CREAT, S_IRWXU, &mgrAttr);
  if (mgrQueue == -1) {
    LOG_E("Unable to open mgrQueue: %d", errno);
    return;
  }

#else
  esp_pthread_init();

  if (mgrQueue == nullptr) {
    mgrQueue = xQueueCreate(5, sizeof(QueueData));
  } else {
    xQueueReset(mgrQueue);
  }

#endif
}

// Reference-counted: >0 means at least one thread is blocking on mgrQueue
std::atomic<int> relax{0}; // Deprecated; use correlationId instead

/**
 * Request on-demand ASAP retrieval for one itemref and wait for correlated
 * ASAP_READY reply with timeout.
 */
auto PageLocs::retrieveAsap(int16_t itemrefIndex) -> bool {
  // Hardening #1: Use correlation IDs for safe request/response matching
  if (!controlTask) return false;

  uint32_t requestId = telemetry.nextRequestId.fetch_add(1);
  telemetry.asapRequestsSubmitted.fetch_add(1);

  // SHOW_IT("retrieveAsap: Sending GET_ASAP with correlationId=%u", requestId);
  PageLocsControl::QueueData cmd{.req           = PageLocsControl::Req::GET_ASAP,
                                 .itemrefIndex  = itemrefIndex,
                                 .itemrefCount  = 0,
                                 .correlationId = requestId};

  if (PageLocsControl::send(cmd) < 0) {
    LOG_W("retrieveAsap: failed to send GET_ASAP");
    telemetry.queueSendFailures.fetch_add(1);
    return false;
  }

  // Callers of retrieveAsap() hold PageLocs::mutex while waiting for ASAP_READY.
  // Allow retriever inserts to bypass mutex acquisition in that specific phase.
  relax.fetch_add(1, std::memory_order_relaxed);

  bool gotReply = false;
  bool matched  = false;
  QueueData queueData;

#if EPUB_LINUX_BUILD
  timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  constexpr int64_t ASAP_REPLY_TIMEOUT_MS = 60000;
  const int64_t nsecTotal = static_cast<int64_t>(ts.tv_nsec) + ASAP_REPLY_TIMEOUT_MS * 1000000LL;
  ts.tv_sec += static_cast<time_t>(nsecTotal / 1000000000LL);
  ts.tv_nsec = static_cast<long>(nsecTotal % 1000000000LL);

  while (mq_timedreceive(mgrQueue, (char *)&queueData, sizeof(queueData), nullptr, &ts) >= 0) {
    if (queueData.correlationId == requestId && queueData.req == Req::ASAP_READY) {
      gotReply = true;
      matched  = true;
      break;
    } else {
      LOG_W("retrieveAsap: mismatched reply (wanted corrId=%u, got req=%d corrId=%u itemref=%d)",
            requestId, static_cast<int>(queueData.req), queueData.correlationId,
            queueData.itemrefIndex);
      telemetry.asapRepliesMismatched.fetch_add(1);
    }
  }
#else
  constexpr TickType_t ASAP_REPLY_TIMEOUT = pdMS_TO_TICKS(60000);

  while (receive(queueData, ASAP_REPLY_TIMEOUT) == pdTRUE) {
    if (queueData.correlationId == requestId && queueData.req == Req::ASAP_READY) {
      gotReply = true;
      matched  = true;
      break;
    } else {
      LOG_W("retrieveAsap: mismatched reply (wanted corrId=%u, got req=%d corrId=%u itemref=%d)",
            requestId, static_cast<int>(queueData.req), queueData.correlationId,
            queueData.itemrefIndex);
      telemetry.asapRepliesMismatched.fetch_add(1);
    }
  }
#endif

  if (!gotReply) {
    telemetry.asapReplyTimeouts.fetch_add(1);
    LOG_W("retrieveAsap: timeout waiting for ASAP reply "
          "(itemref=%d, corrId=%u, relax=%d, submitted=%llu matched=%llu mismatched=%llu "
          "timeouts=%llu)",
          itemrefIndex, requestId, relax.load(std::memory_order_relaxed),
          static_cast<unsigned long long>(telemetry.asapRequestsSubmitted.load()),
          static_cast<unsigned long long>(telemetry.asapRepliesMatched.load()),
          static_cast<unsigned long long>(telemetry.asapRepliesMismatched.load()),
          static_cast<unsigned long long>(telemetry.asapReplyTimeouts.load()));
    relax.fetch_sub(1, std::memory_order_relaxed);
    return false;
  }

  if (matched) {
    telemetry.asapRepliesMatched.fetch_add(1);
  }

  relax.fetch_sub(1, std::memory_order_relaxed);

  return matched;
}

/**
 * Stop control/retriever pipeline, wait for STOPPED, and flush deferred save.
 */
auto PageLocs::stopControlTask() -> void {

  if (isRunning()) {
    LOG_D("Sending STOP");
    if (PageLocsControl::send({.req = PageLocsControl::Req::STOP}) < 0) {
      LOG_E("stopControlTask: failed to send STOP");
      telemetry.queueSendFailures.fetch_add(1);
      telemetry.stopTaskFailures.fetch_add(1);
      // Cannot safely join the control thread if STOP was not delivered.
      // Leave the task running and let caller decide fallback behavior.
      return;
    }

    QueueData queueData;
    bool stopped = false;

    // Hardening #3: Bounded wait with retries and timeout
    auto start_time = std::chrono::steady_clock::now();
    int retry_count = 0;

    while (!stopped && retry_count < STOP_MAX_RETRIES) {
      // SHOW_IT("==> Waiting for STOPPED (retry %d/%d)... <==", retry_count + 1, STOP_MAX_RETRIES);

      // Try to receive with bounded timeout
      if (receive(queueData, STOP_RETRY_TIMEOUT_MS)) {
        if (queueData.req == Req::STOPPED) {
          stopped = true;
          LOG_D("-> STOPPED received <-");
        } else if ((queueData.req == Req::PERCENT) || (queueData.req == Req::ASAP_READY)) {
          // Expected stale manager traffic while shutting down; ignore.
          LOG_D("Ignoring stale message type %d while waiting for STOPPED", (int)queueData.req);
        } else {
          LOG_W("Received unexpected message type: %d, discarding and retrying",
                (int)queueData.req);
          telemetry.stopTaskRetries++;
        }
      } else {
        // Timeout occurred
        LOG_W("STOP wait timeout (retry %d/%d)", retry_count + 1, STOP_MAX_RETRIES);
        telemetry.stopTaskTimeouts++;
      }

      retry_count++;

      // Check hard limit
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start_time)
                            .count();
      if (elapsed_ms > STOP_TOTAL_TIMEOUT_MS && !stopped) {
        LOG_E("STOP hard timeout reached (%lld ms) - forcefully terminating", elapsed_ms);
        telemetry.stopTaskFailures++;
        break;
      }
    }

    if (!stopped) {
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start_time)
                            .count();
      LOG_E("Failed to receive STOPPED message after %d retries, %lld ms elapsed - forcing cleanup",
            STOP_MAX_RETRIES, elapsed_ms);
      telemetry.stopTaskFailures++;
      // Fallback: Force exit without confirmation (recovery path)
    }
    // Hardening #3: Bounded wait for task exit with timeout
    LOG_D("Waiting for task exit...");
    controlTask->waitForExit();
    controlTask.reset();
    LOG_D("Control task cleaned up");
  }
  // Save after worker teardown so SD/FATFS file locks are released.
  if (pendingSave) {
    pendingSave = false;
    save(currentFilename);
  }

  controlTaskReadyToBeStopped = false;
}

/**
 * Return completed page count when available; otherwise request and return
 * current percentage from control task.
 */
auto PageLocs::getPageCountOrPercent() -> int16_t {
  if (isControlTaskReadyToBeStopped()) stopControlTask();

  // LOG_I("getPageCountOrPercent: completed=%d aborted=%d controlTask=%s itemsSet.size()=%d "
  //       "itemCount=%d",
  //       completed, aborted, controlTask ? "yes" : "no", static_cast<int>(itemsSet.size()),
  //       static_cast<int>(itemCount));

  if (completed) return pageCount;

  if (aborted) return -1;

  if (!controlTask) return -1;

  // Do not consume mgrQueue here while computation is in progress.
  // Navigation ASAP waits use the same queue; competing consumers can starve
  // correlated ASAP replies and cause avoidable navigation timeouts.
  {
    std::scoped_lock guard(mutex);
    if (itemCount <= 0) return 0;

    int16_t percent = static_cast<int16_t>((itemsSet.size() * 100) / itemCount);

    // LOG_I("Progress: %d%% (%d/%d items)", percent, static_cast<int>(itemsSet.size()),
    //       static_cast<int>(itemCount));

    if (percent < 0) percent = 0;
    if (percent > 99) percent = 99;
    return percent;
  }
}

/**
 * Entry point for switching to a new book context.
 */
auto PageLocs::startNewDocument(EPubPtr &epub, int16_t itemrefIndex) -> void {
  stopControlTask();

  currentFilename = epub->getCurrentFilename();
  checkForFormatChanges(epub, itemrefIndex, !load(currentFilename));
}

/**
 * Insert one computed page boundary into shared map/set under strict locking.
 */
auto PageLocs::insert(PageId &id, PageInfo &info) -> void {
  // LOG_I("Inserting page: PageId{itemref=%d offset=%d} PageInfo{size=%d pageNumber=%d}",
  //       id.itemrefIndex, id.offset, info.size, info.pageNumber);
  if (controlTask && (relax.load(std::memory_order_relaxed) > 0)) {
    // A waiter is blocked in retrieveAsap() while holding PageLocs::mutex.
    // Insert directly to avoid starving retriever progress until timeout.
    auto inserted = pagesMap.insert(std::make_pair(id, info));
    if (inserted.second) {
      itemsSet.insert(id.itemrefIndex);
      generatedPageEntryCount.fetch_add(1, std::memory_order_relaxed);
      telemetry.mapInsertions.fetch_add(1);
    }
    return;
  }

  bool contentious = false;
  int attempts     = 0;

  while (true) {
    if (mutex.try_lock_for(std::chrono::milliseconds(2))) {
      auto inserted = pagesMap.insert(std::make_pair(id, info));
      if (inserted.second) {
        itemsSet.insert(id.itemrefIndex);
        generatedPageEntryCount.fetch_add(1, std::memory_order_relaxed);
        telemetry.mapInsertions.fetch_add(1);
      }
      mutex.unlock();
      if (contentious) {
        telemetry.mapLockContentions.fetch_add(1);
      }
      break;
    }
    attempts++;
    contentious = (attempts > 1);
  }
}

/**
 * Lookup helper that triggers ASAP retrieval when data is missing and
 * computation is still active.
 */
auto PageLocs::checkAndFind(const PageId &pageId) -> PageLocs::PagesMap::iterator {
  if (pageId.itemrefIndex < 0) return pagesMap.end();

  PagesMap::iterator it = pagesMap.find(pageId);
  if (!completed && (it == pagesMap.end())) {
    if (retrieveAsap(pageId.itemrefIndex)) it = pagesMap.find(pageId);
  }
  return it;
}

/**
 * Resolve next navigable page from current page id, optionally stepping by
 * multiple pages and crossing item boundaries.
 */
auto PageLocs::getNextPageId(const PageId &pageId, int16_t count) -> const PageId * {

  if (isControlTaskReadyToBeStopped()) stopControlTask();

  {
    std::scoped_lock guard(mutex);
    PagesMap::iterator it = checkAndFind(pageId);
    if (it == pagesMap.end()) {
      it = checkAndFind(PageId(0, 0));
    } else {
      PageId id = pageId;
      bool done = false;
      for (int16_t cptr = count; cptr > 0; cptr--) {
        PagesMap::iterator prev = it;
        do {
          id.offset += abs(it->second.size);
          it = pagesMap.find(id);
          if (it == pagesMap.end()) {
            // We have reached the end of the current item. Move to the next
            // item and try again.
            id.itemrefIndex += 1;
            id.offset = 0;
            it        = checkAndFind(id);
            if (it == pagesMap.end()) {
              // We have reached the end of the list. If stepping one page at
              // a time, go to the first page.
              it   = (count > 1) ? prev : checkAndFind(PageId(0, 0));
              done = true;
            }
          }
        } while (!done && (it->second.size < 0));
        if (done) break;
      }
    }
    return (it == pagesMap.end()) ? nullptr : &it->first;
  }
}

/**
 * Resolve previous navigable page from current page id, optionally stepping by
 * multiple pages and crossing item boundaries.
 */
auto PageLocs::getPrevPageId(const PageId &pageId, int count) -> const PageId * {

  if (isControlTaskReadyToBeStopped()) stopControlTask();

  {
    std::scoped_lock guard(mutex);

    PagesMap::iterator it = checkAndFind(pageId);
    if (it == pagesMap.end()) {
      it = checkAndFind(PageId(0, 0));
    } else {
      PageId id           = it->first;
      auto stepBackNoWrap = [&](PagesMap::iterator &iter, PageId &page) -> bool {
        if (iter == pagesMap.begin()) return false;
        iter--;
        page = iter->first;
        return true;
      };

      bool done = false;
      for (int16_t cptr = count; cptr > 0; cptr--) {
        do {
          if (id.offset == 0) {
            int16_t targetItemref = id.itemrefIndex;

            if (targetItemref == 0) {
              if (count == 1) {
                if (itemCount > 0)
                  targetItemref = itemCount - 1;
                else
                  done = true;
              } else
                done = true;
            } else
              targetItemref--;

            if (!done) {
              PagesMap::iterator firstInItem = checkAndFind(PageId(targetItemref, 0));
              if ((firstInItem != pagesMap.end()) &&
                  (firstInItem->first.itemrefIndex == targetItemref)) {
                PagesMap::iterator nextItem =
                    pagesMap.lower_bound(PageId(static_cast<int16_t>(targetItemref + 1), 0));
                if (nextItem == pagesMap.begin()) {
                  it   = pagesMap.end();
                  done = true;
                } else {
                  it = nextItem;
                  it--;
                  if (it->first.itemrefIndex != targetItemref) {
                    it   = pagesMap.end();
                    done = true;
                  } else {
                    id = it->first;
                  }
                }
              } else {
                it   = pagesMap.end();
                done = true;
              }
            }
          } else {
            if (!stepBackNoWrap(it, id)) {
              it   = pagesMap.end();
              done = true;
            }
          }

        } while (!done && (it->second.size < 0));
        if (done) break;
      }
    }
    return (it == pagesMap.end()) ? nullptr : &it->first;
  }
}

/**
 * Resolve the page entry containing a specific item offset.
 */
auto PageLocs::getPageId(const PageId &pageId) -> const PageId * {

  if (isControlTaskReadyToBeStopped()) stopControlTask();

  {
    std::scoped_lock guard(mutex);

    PagesMap::iterator it     = checkAndFind(PageId(pageId.itemrefIndex, 0));
    PagesMap::iterator result = pagesMap.end();
    while ((it != pagesMap.end()) && (it->first.itemrefIndex == pageId.itemrefIndex)) {
      if ((it->first.offset == pageId.offset) ||
          ((it->first.offset < pageId.offset) &&
           ((it->first.offset + abs(it->second.size)) > pageId.offset))) {
        result = it;
        break;
      }
      ++it;
    }
    return (result == pagesMap.end()) ? nullptr : &result->first;
  }
}

/**
 * Finalize successful computation state, number pages, and mark deferred save.
 */
auto PageLocs::computationCompleted() -> void {
  std::scoped_lock guard(mutex);

  if (!completed) {
    int16_t pageNbr = 0;
    for (auto &entry : pagesMap) {
      if (entry.second.size >= 0) entry.second.pageNumber = pageNbr++;
    }

    pageCount = pageNbr;

    // Defer the save until stopControlTask() after control/retriever shutdown.
    pendingSave = true;

    // show();

    completed                   = true;
    controlTaskReadyToBeStopped = true;
    computationDone.store(true);

    // This cannot be done here!!!!
    // epub->toc->save(currentFilename);

    eventMgr.setStayOn(false);

    // #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    //   ESP::show_heaps_info();
    //   RetrieveQueueData retrieve_queue_data = {
    //     .req = RetrieveReq::SHOW_HEAP,
    //     .itemrefIndex = 0
    //   };
    //   LOG_D("Sending SHOW_HEAP to Retriever");
    //   QUEUE_SEND(retrieve_queue, retrieve_queue_data, 0);
    // #endif
  }
}

/**
 * Mark computation as aborted and notify user-facing layer with reason.
 */
auto PageLocs::computationAborted(std::string reason) -> void {
  MsgViewer::show(MsgViewer::MsgType::ALERT, true, false, "Pages Computation Aborted",
                  "Unable to complete pages location computation for this book. "
                  "This book cannot be read on this device. Reason: %s.",
                  reason.c_str());
  controlTaskReadyToBeStopped = true;
  aborted                     = true;
  completed                   = true;
  pendingSave                 = false;

  eventMgr.setStayOn(false);
}

#if DEBUGGING
auto PageLocs::show() -> void {
  std::cout << "----- Page Locations -----" << std::endl;
  for (auto &entry : pagesMap) {
    std::cout << " idx: " << entry.first.itemrefIndex << " off: " << entry.first.offset
              << " siz: " << entry.second.size << " pg: " << entry.second.pageNumber << std::endl;
  }
  std::cout << "----- End Page Locations -----" << std::endl;
}
#endif

/**
 * @brief Checks if page locations need to be recalculated due to format changes or missing TOC
 *
 * This method determines whether page locations should be recomputed by comparing the current
 * format parameters with those stored in the epub, or if forced recalculation is requested.
 * If recalculation is needed, it stops any existing control task, clears current page locations,
 * sets up new page computation, and initiates a new document processing task.
 *
 * @param epub Reference to the EPub object containing the document and format parameters
 * @param itemrefIndex Index of the current item reference in the epub manifest
 * @param force If true, forces recalculation regardless of format parameter changes
 *
 * @note This method will also trigger recalculation if no control task is running and
 *       no table of contents exists for the current filename
 * @note Sets the event manager to stay on during the recalculation process
 */
auto PageLocs::checkForFormatChanges(EPubPtr &epub, int16_t itemrefIndex, bool force) -> void {
  // Keep the target filename in sync even when callers invoke this directly
  // (without going through startNewDocument).
  currentFilename = epub->getCurrentFilename();
  itemCount       = epub->getItemCount();

  if (force ||
      (memcmp(epub->getBookFormatParams(), &currentFormatParams, sizeof(currentFormatParams)) !=
       0) ||
      (!controlTask && !TOC::exists(epub->getCurrentFilename()))) {

    showLoadIcon(Dim(500, 500));

    LOG_D("==> Page locations recalc. <==");

    stopControlTask();

    if (isRunning()) {
      LOG_E("Unable to stop existing control task. Aborting page locations computation.");
      return;
    }

    clear();

    epub->getFonts().clearGlyphCaches();

    setupPagesComputation(epub);

    currentFormatParams = *epub->getBookFormatParams();

    LOG_D("Starting page locations computation for itemref index %d with format params: ident=%d, "
          "orientation=%d, "
          "showTitle=%d, showPictures=%d, fontSize=%d, useFontsInBook=%d, font=%d",
          itemrefIndex, currentFormatParams.ident, currentFormatParams.orientation,
          currentFormatParams.showTitle, currentFormatParams.showPictures,
          currentFormatParams.fontSize, currentFormatParams.useFontsInBook,
          currentFormatParams.font);

    // SHOW_IT("startNewDocument: Sending START_DOCUMENT");
    if (PageLocsControl::send({.req          = PageLocsControl::Req::START_DOCUMENT,
                               .itemrefIndex = itemrefIndex,
                               .itemrefCount = epub->getItemCount()}) < 0) {
      LOG_E("checkForFormatChanges: failed to send START_DOCUMENT");
      telemetry.queueSendFailures.fetch_add(1);
      return;
    }

    eventMgr.setStayOn(true);
  }
}

/**
 * Load persisted page locations and associated format metadata from .locs file.
 */
auto PageLocs::load(const std::string &epubFilename) -> bool {
  std::string filename = epubFilename.substr(0, epubFilename.find_last_of('.')) + ".locs";
  std::ifstream file(filename, std::ios::in | std::ios::binary);

  LOG_D("Loading pages location from file %s.", filename.c_str());

  int8_t version;
  int16_t pgCount;

  if (!file.is_open()) {
    LOG_I("Unable to open pages location file '%s': errno=%d (%s). Calculating locations...",
          filename.c_str(), errno, std::strerror(errno));
    return false;
  }

  bool ok = false;
  while (true) {
    if (file.read(reinterpret_cast<char *>(&version), 1).fail()) break;
    if (version != LOCS_FILE_VERSION) break;

    if (file.read(reinterpret_cast<char *>(&currentFormatParams), sizeof(currentFormatParams))
            .fail())
      break;
    if (file.read(reinterpret_cast<char *>(&pgCount), sizeof(pgCount)).fail()) break;

    pagesMap.clear();
    generatedPageEntryCount.store(0);

    int16_t pageNbr = 0;

    for (int16_t i = 0; i < pgCount; ++i) {
      PageId pageId;
      PageInfo pageInfo;

      if (file.read(reinterpret_cast<char *>(&pageId.itemrefIndex), sizeof(pageId.itemrefIndex))
              .fail())
        break;
      if (file.read(reinterpret_cast<char *>(&pageId.offset), sizeof(pageId.offset)).fail()) break;
      if (file.read(reinterpret_cast<char *>(&pageInfo.size), sizeof(pageInfo.size)).fail()) break;
      pageInfo.pageNumber = (pageInfo.size >= 0) ? pageNbr++ : -1;

      pageLocs.insert(pageId, pageInfo);
    }

    pageCount = pageNbr;
    ok        = !file.fail();
    break;
  }

  file.close();

  if (!ok) {
    LOG_E("Page locations load failed for '%s' (fail=%d bad=%d eof=%d)", filename.c_str(),
          file.fail() ? 1 : 0, file.bad() ? 1 : 0, file.eof() ? 1 : 0);
  }

  LOG_D("Page locations load %s.", ok ? "Success" : "Error");

  completed = ok;

  return ok;
}

/**
 * Persist current page locations and format metadata to .locs file.
 */
auto PageLocs::save(const std::string &epubFilename) -> bool {
  std::string filename = epubFilename.substr(0, epubFilename.find_last_of('.')) + ".locs";
  std::ofstream file(filename, std::ios::out | std::ios::binary);

  LOG_D("Saving pages location to file %s", filename.c_str());

  if (!file.is_open()) {
    LOG_E("Not able to open pages location file '%s': errno=%d (%s)", filename.c_str(), errno,
          std::strerror(errno));
    return false;
  }

  int16_t savedPageCount = pagesMap.size();

  while (true) {
    if (file.write(reinterpret_cast<const char *>(&LOCS_FILE_VERSION), 1).fail()) break;
    if (file.write(reinterpret_cast<const char *>(&currentFormatParams),
                   sizeof(currentFormatParams))
            .fail())
      break;
    if (file.write(reinterpret_cast<const char *>(&savedPageCount), sizeof(savedPageCount)).fail())
      break;

    for (auto &pageMapEntry : pagesMap) {
      if (file.write(reinterpret_cast<const char *>(&pageMapEntry.first.itemrefIndex),
                     sizeof(pageMapEntry.first.itemrefIndex))
              .fail())
        break;
      if (file.write(reinterpret_cast<const char *>(&pageMapEntry.first.offset),
                     sizeof(pageMapEntry.first.offset))
              .fail())
        break;
      if (file.write(reinterpret_cast<const char *>(&pageMapEntry.second.size),
                     sizeof(pageMapEntry.second.size))
              .fail())
        break;
    }

    break;
  }

  bool res = !file.fail();
  file.close();

  if (!res) {
    LOG_E("Page locations save failed for '%s' (fail=%d bad=%d eof=%d)", filename.c_str(),
          file.fail() ? 1 : 0, file.bad() ? 1 : 0, file.eof() ? 1 : 0);
  }

  LOG_D("Page locations save %s.", res ? "Success" : "Error");

  return res;
}
