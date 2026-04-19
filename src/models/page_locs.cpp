// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PAGE_LOCS__ 1
#include "models/page_locs.hpp"

#include "controllers/event_mgr.hpp"
#include "models/config.hpp"
#include "models/page_locs_control.hpp"

#include "viewers/book_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/page.hpp"

#include <atomic>
#include <fstream>
#include <ios>
#include <iostream>

#if EPUB_LINUX_BUILD
  #include <chrono>
#else
  #include <esp_pthread.h>
#endif

#if EPUB_INKPLATE_BUILD
  QueueHandle_t PageLocs::mgrQueue{nullptr};
#else
  #include <mqueue.h>
  mqd_t PageLocs::mgrQueue{-1};
#endif

auto PageLocs::setupPagesComputation(EPubPtr &epub) -> void {

  stopControlTask();

  completed                   = false;
  aborted                     = false;
  controlTaskReadyToBeStopped = false;

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

    if (mgrQueue == nullptr) mgrQueue = xQueueCreate(5, sizeof(QueueData));

  #endif
}

std::atomic<bool> relax{false};

auto PageLocs::retrieveAsap(int16_t itemrefIndex) -> bool {
  SHOW_IT("retrieveAsap: Sending GET_ASAP");
  PageLocsControl::send(
      {.req = PageLocsControl::Req::GET_ASAP, .itemrefIndex = itemrefIndex, .itemrefCount = 0});

  relax = true;
  QueueData queueData;
  LOG_D("==> Waiting for answer... <==");
  receive(queueData);
  LOG_D("-> %s <-", queueData.req == Req::ASAP_READY ? "ASAP_READY" : "ERROR!!!");
  relax = false;

  return true;
}

auto PageLocs::stopControlTask() -> void {

  if (controlTask) {
    SHOW_IT("Sending STOP");
    PageLocsControl::send({.req = PageLocsControl::Req::STOP});

    QueueData queueData;
    SHOW_IT("==> Waiting for STOPPED... <==");
    receive(queueData);
    SHOW_IT("-> %s <-", (queueData.req == Req::STOPPED) ? "STOPPED" : "ERROR!!!");

    controlTask->waitForExit();
    controlTask.reset();
  }

  controlTaskReadyToBeStopped = false;
}

auto PageLocs::getPageCountOrPercent() -> int16_t {
  if (controlTaskReadyToBeStopped) stopControlTask();

  if (completed) return pageCount;

  if (aborted) return -1;

  if (controlTask) {
    SHOW_IT("getPageCountOrPercent: Sending PERCENT");
    PageLocsControl::send({.req = PageLocsControl::Req::PERCENT});

    relax = true;
    QueueData queueData;
    SHOW_IT("==> Waiting for answer... <==");
    receive(queueData);
    SHOW_IT("-> %s <-", queueData.req == Req::PERCENT ? "PERCENT" : "ERROR!!!");
    relax = false;

    return queueData.itemrefIndex;
  } else {
    return -1;
  }
}

auto PageLocs::startNewDocument(EPubPtr &epub, int16_t itemrefIndex) -> void {
  if (controlTask) stopControlTask();

  currentFilename = epub->getCurrentFilename();
  // checkForFormatChanges(epub, itemrefIndex, !load(currentFilename));
}

auto PageLocs::insert(PageId &id, PageInfo &info) -> bool {
  if (controlTask) {
    while (true) {
      if (relax) {
        // The pageLocs class is still in control of the mutex, but is waiting
        // for the completion of an GET_ASAP item. As such, it is safe to insert
        // a new page info in the list.
        LOG_D("Relaxed page info insert...");
        pagesMap.insert(std::make_pair(id, info));
        itemsSet.insert(id.itemrefIndex);
        break;
      } else {
        if (mutex.try_lock_for(std::chrono::milliseconds(10))) {
          pagesMap.insert(std::make_pair(id, info));
          itemsSet.insert(id.itemrefIndex);
          mutex.unlock();
          break;
        }
      }
    }
    return true;
  }
  return false;
}

auto PageLocs::checkAndFind(const PageId &pageId) -> PageLocs::PagesMap::iterator {
  PagesMap::iterator it = pagesMap.find(pageId);
  if (!completed && (it == pagesMap.end())) {
    if (retrieveAsap(pageId.itemrefIndex)) it = pagesMap.find(pageId);
  }
  return it;
}

auto PageLocs::getNextPageId(const PageId &pageId, int16_t count) -> const PageId * {
  if (controlTaskReadyToBeStopped) stopControlTask();

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
            // item and try again
            id.itemrefIndex += 1;
            id.offset = 0;
            it        = checkAndFind(id);
            if (it == pagesMap.end()) {
              // We have reached the end of the list. If stepping one page at a time, go
              // to the first page
              it   = (count > 1) ? prev : checkAndFind(PageId(0, 0));
              done = true;
            }
          }
        } while ((it->second.size < 0) && !done);
        if (done) break;
      }
    }
    return (it == pagesMap.end()) ? nullptr : &it->first;
  }
}

auto PageLocs::getPrevPageId(const PageId &pageId, int count) -> const PageId * {
  if (controlTaskReadyToBeStopped) stopControlTask();

  {
    std::scoped_lock guard(mutex);

    PagesMap::iterator it = checkAndFind(pageId);
    if (it == pagesMap.end()) {
      it = checkAndFind(PageId(0, 0));
    } else {
      PageId id = it->first;

      bool done = false;
      for (int16_t cptr = count; cptr > 0; cptr--) {
        do {
          if (id.offset == 0) {
            if (id.itemrefIndex == 0) {
              if (count == 1)
                id.itemrefIndex = itemCount - 1;
              else
                done = true;
            } else
              id.itemrefIndex--;

            if (itemsSet.find(id.itemrefIndex) == itemsSet.end()) {
              retrieveAsap(id.itemrefIndex);
            }
          }

          if (!done) {
            if (it == pagesMap.begin()) it = pagesMap.end();
            it--;
            id = it->first;
          }
        } while ((it->second.size < 0) && !done);
        if (done) break;
      }
    }
    return (it == pagesMap.end()) ? nullptr : &it->first;
  }
}

auto PageLocs::getPageId(const PageId &pageId) -> const PageId * {
  if (controlTaskReadyToBeStopped) stopControlTask();

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
      it++;
    }
    return (result == pagesMap.end()) ? nullptr : &result->first;
  }
}

auto PageLocs::computationCompleted() -> void {
  std::scoped_lock guard(mutex);

  if (!completed) {
    int16_t pageNbr = 0;
    for (auto &entry : pagesMap) {
      if (entry.second.size >= 0) entry.second.pageNumber = pageNbr++;
    }

    pageCount = pageNbr;

    save(currentFilename);

    // show();

    completed                   = true;
    controlTaskReadyToBeStopped = true;

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

auto PageLocs::computationAborted(std::string reason) -> void {
  MsgViewer::show(MsgViewer::MsgType::ALERT, true, false, "Pages Computation Aborted",
                  "Unable to complete pages location computation for this book. "
                  "This book cannot be read on this device. Reason: %s.",
                  reason.c_str());
  controlTaskReadyToBeStopped = true;
  aborted                     = true;
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

auto PageLocs::checkForFormatChanges(EPubPtr &epub, int16_t itemrefIndex, bool force) -> void {
  if (force ||
      (memcmp(epub->getBookFormatParams(), &currentFormatParams, sizeof(currentFormatParams)) !=
       0) ||
      !epub->toc->load(epub)) {

    LOG_D("==> Page locations recalc. <==");

    if (controlTask) stopControlTask();

    clear();

    setupPagesComputation(epub);

    currentFormatParams = *epub->getBookFormatParams();

    SHOW_IT("startNewDocument: Sending START_DOCUMENT");
    PageLocsControl::send({.req          = PageLocsControl::Req::START_DOCUMENT,
                           .itemrefIndex = itemrefIndex,
                           .itemrefCount = epub->getItemCount()});

    eventMgr.setStayOn(true);
  }
}

auto PageLocs::load(const std::string &epubFilename) -> bool {
  std::string filename = epubFilename.substr(0, epubFilename.find_last_of('.')) + ".locs";
  std::ifstream file(filename, std::ios::in | std::ios::binary);

  LOG_D("Loading pages location from file %s.", filename.c_str());

  int8_t version;
  int16_t pgCount;

  if (!file.is_open()) {
    LOG_I("Unable to open pages location file. Calculating locations...");
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

    int16_t pageNbr = 0;

    for (int16_t i = 0; i < pgCount; i++) {
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

  LOG_D("Page locations load %s.", ok ? "Success" : "Error");

  completed = ok;

  return ok;
}

auto PageLocs::save(const std::string &epubFilename) -> bool {
  std::string filename = epubFilename.substr(0, epubFilename.find_last_of('.')) + ".locs";
  std::ofstream file(filename, std::ios::out | std::ios::binary);

  LOG_D("Saving pages location to file %s", filename.c_str());

  if (!file.is_open()) {
    LOG_E("Not able to open pages location file.");
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

  LOG_D("Page locations save %s.", res ? "Success" : "Error");

  return res;
}
