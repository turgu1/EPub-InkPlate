#include "page_locs_state.hpp"

#include "models/page_locs.hpp"

#if EPUB_INKPLATE_BUILD
  QueueHandle_t PageLocsState::stateQueue{nullptr};
#else
  mqd_t PageLocsState::stateQueue{-1};
#endif

auto PageLocsState::setup(const std::string &epubFilename) -> bool {
  if (!retrieverTask.setup(epubFilename)) {
    LOG_E("Unable to setup retriever task");
    return false;
  }

  #if EPUB_LINUX_BUILD
    mq_unlink("/state");

    stateQueue = mq_open("/state", O_RDWR | O_CREAT, S_IRWXU, &stateAttr);
    if (stateQueue == -1) {
      LOG_E("Unable to open stateQueue: %d", errno);
      return false;
    }

    stateThread = std::thread(&PageLocsState::task, this);
  #else
    esp_pthread_init();

    if (stateQueue == nullptr) stateQueue = xQueueCreate(5, sizeof(PageLocsState::QueueData));
    if (stateQueue == nullptr) {
      LOG_E("Unable to create stateQueue");
      return false;
    }

    auto cfg        = esp_pthread_get_default_config();
    cfg.thread_name = "stateTask";
    cfg.pin_to_core = 0;
    cfg.stack_size  = 10 * 1024;
    cfg.prio        = configMAX_PRIORITIES - 2;
    cfg.inherit_cfg = true;

    esp_pthread_set_cfg(&cfg);
    stateThread = std::thread(&PageLocsState::task, this);

  #endif

  return true;
}

void PageLocsState::waitForExit() {
  stateThread.join();
  stateThread.~thread();
}

void PageLocsState::abort() {
  LOG_D("abortThreads: Sending ABORT to Retriever");
  PageLocsRetriever::send({.req = PageLocsRetriever::Req::ABORT});

  retrieverTask.waitForExit();
}

/**
 * @brief Request next item to be retrieved
 *
 * This function is called to identify and send the
 * next request for retrieval of pages location. It also
 * identify when the whole process is completed, as all items from
 * the document have been done. It will then send this information
 * to the appliction through the Mgr queue.
 *
 * When this function is called, the retrieval task is waiting for
 * the next task to do.
 *
 * @param itemref The last itemref index that was processed
 */
void PageLocsState::requestNextItem(int16_t itemref, bool alreadySentToMgr) {
  if (asapItemref != -1) { // is there an urgent spine to do?
    if (itemref == asapItemref) {
      asapItemref = -1;
      if (!alreadySentToMgr) {
        PageLocs::send({.req = PageLocs::Req::ASAP_READY, .itemrefIndex = itemref});
        LOG_D("Sent ASAP_READY to Mgr");
      }
    } else {
      waitingForItemref = asapItemref;
      asapItemref       = -1;
      PageLocsRetriever::send(
          {.req = PageLocsRetriever::Req::GET_ASAP, .itemrefIndex = waitingForItemref});
      retrieverIdle = false;
      LOG_D("Sent GET_ASAP to Retriever");
      return;
    }
  }
  if (nextItemrefToGet != -1) {
    waitingForItemref = nextItemrefToGet;
    nextItemrefToGet  = -1;
    PageLocsRetriever::send(
        {.req = PageLocsRetriever::Req::RETRIEVE_ITEM, .itemrefIndex = waitingForItemref});
    retrieverIdle = false;
    LOG_D("Sent RETRIEVE_ITEM to Retriever");
  } else {
    int16_t newref;
    if (itemref != -1) {
      newref = (itemref + 1) % itemrefCount;
    } else {
      itemref = 0;
      newref  = 0;
    }
    while ((bitset[newref >> 3] & (1 << (newref & 7))) != 0) {
      newref = (newref + 1) % itemrefCount;
      if (newref == itemref) break;
    }
    if (newref != itemref) {
      waitingForItemref = newref;
      PageLocsRetriever::send(
          {.req = PageLocsRetriever::Req::RETRIEVE_ITEM, .itemrefIndex = waitingForItemref});
      retrieverIdle = false;
      LOG_D("Sent RETRIEVE_ITEM to Retriever");
    } else {
      pageLocs.computationCompleted();
      retrieverIdle = true;
    }
  }
}

void PageLocsState::task() {
  for (;;) {
    LOG_D("==> Waiting for request... <==");
    QueueData stateQueueData;

    if (receive(stateQueueData) == -1) {
      LOG_E("Receive error: %d: %s", errno, strerror(errno));
    } else
      switch (stateQueueData.req) {
      case Req::NONE:
        break;
      case Req::ABORT:
        abort();
        return;

      case Req::STOP:
        LOG_D("-> STOP <-");
        itemrefCount    = -1;
        forgetRetrieval = true;
        if (bitset != nullptr) {
          delete[] bitset;
          bitset = nullptr;
        }
        if (retrieverIdle) {
          PageLocs::send({.req = PageLocs::Req::STOPPED, .itemrefIndex = 0});
        } else {
          stopping = true;
        }
        break;

      case Req::START_DOCUMENT:
        LOG_D("-> START_DOCUMENT <-");
        if (bitset) delete[] bitset;
        itemrefCount   = stateQueueData.itemrefCount;
        itemsDoneCount = 0;
        // ESP_LOGI(TAG,"itemsDoneCount = %" PRIi16 " of %" PRIi16, itemsDoneCount,
        // itemrefCount);
        bitsetSize = (itemrefCount + 7) >> 3;
        bitset     = new uint8_t[bitsetSize];
        if (bitset) {
          memset(bitset, 0, bitsetSize);
          if (waitingForItemref == -1) {
            waitingForItemref = stateQueueData.itemrefIndex;
            forgetRetrieval   = false;
            PageLocsRetriever::send(
                {.req = PageLocsRetriever::Req::RETRIEVE_ITEM, .itemrefIndex = waitingForItemref});
            LOG_D("Sent RETRIEVE_ITEM to retriever");
          } else {
            forgetRetrieval  = true;
            nextItemrefToGet = stateQueueData.itemrefIndex;
          }
          retrieverIdle = false;
        } else {
          itemrefCount    = -1;
          retrieverIdle   = true;
          forgetRetrieval = true;
        }
        break;

      case Req::GET_ASAP:
        LOG_D("-> GET_ASAP <-");
        // Mgr request a specific item. If document retrieval not started,
        // return a negative value.
        // If already done, let it know it a.s.a.p. If currently being processed,
        // keep a mark when it will be back. If not, queue the request.
        if (itemrefCount == -1) {
          PageLocs::send(
              {.req          = PageLocs::Req::ASAP_READY,
               .itemrefIndex = static_cast<int16_t>(-(stateQueueData.itemrefIndex + 1))});
          LOG_D("Sent ASAP_READY to Mgr");
        } else {
          int16_t itemref = stateQueueData.itemrefIndex;
          if ((bitset[itemref >> 3] & (1 << (itemref & 7))) != 0) {
            PageLocs::send({.req = PageLocs::Req::ASAP_READY, .itemrefIndex = itemref});
            LOG_D("Sent ASAP_READY to Mgr");
          } else if (waitingForItemref != -1) {
            asapItemref = itemref;
          } else {
            asapItemref       = -1;
            waitingForItemref = itemref;
            retrieverIdle     = false;
            PageLocsRetriever::send(
                {.req = PageLocsRetriever::Req::GET_ASAP, .itemrefIndex = itemref});
            LOG_D("Sent GET_ASAP to Retriever");
          }
        }
        break;

      // This is sent by the retrieval task, indicating that an item has been
      // processed.
      case Req::ITEM_READY:
        LOG_D("-> ITEM_READY <-");
        waitingForItemref = -1;
        if (itemrefCount != -1) {
          int16_t itemref = -1;
          if (forgetRetrieval) {
            forgetRetrieval = false;
          } else {
            itemref = stateQueueData.itemrefIndex;
            if (itemref < 0) {
              itemref = -(itemref + 1);
              LOG_E("Unable to retrieve pages location for item %d", itemref);
            }
            if ((bitset[itemref >> 3] & (1 << (itemref & 7))) == 0) {
              bitset[itemref >> 3] |= (1 << (itemref & 7));
              itemsDoneCount += 1;
              // ESP_LOGI(TAG,"itemsDoneCount = %" PRIi16 " of %" PRIi16, itemsDoneCount,
              // itemrefCount);
            }
          }
          if (stopping) {
            stopping      = false;
            retrieverIdle = true;
            PageLocs::send({.req = PageLocs::Req::STOPPED});
          } else {
            requestNextItem(itemref);
          }
        } else {
          if (stopping) {
            stopping      = false;
            retrieverIdle = true;
            PageLocs::send({.req = PageLocs::Req::STOPPED});
          }
        }
        break;

      // This is sent by the retrieval task, indicating that an ASAP item has been
      // processed.
      case Req::ASAP_READY:
        LOG_D("-> ASAP_READY <-");
        waitingForItemref = -1;
        if (itemrefCount != -1) {
          int16_t itemref = stateQueueData.itemrefIndex;
          PageLocs::send({.req = PageLocs::Req::ASAP_READY, .itemrefIndex = itemref});
          LOG_D("Sent ASAP_READY to Mgr");
          if (itemref < 0) {
            itemref = -(itemref + 1);
            LOG_E("Unable to retrieve pages location for item %d", itemref);
          }
          if ((bitset[itemref >> 3] & (1 << (itemref & 7))) == 0) {
            bitset[itemref >> 3] |= (1 << (itemref & 7));
            itemsDoneCount += 1;
            // ESP_LOGI(TAG,"itemsDoneCount = %" PRIi16" of %" PRIi16, itemsDoneCount,
            // itemrefCount);
          }
          if (stopping) {
            stopping      = false;
            retrieverIdle = true;
            PageLocs::send({.req = PageLocs::Req::STOPPED, .itemrefIndex = 0});
          } else {
            requestNextItem(itemref, true);
          }
        } else {
          if (stopping) {
            stopping      = false;
            retrieverIdle = true;
            PageLocs::send({.req = PageLocs::Req::STOPPED, .itemrefIndex = 0});
          }
        }
        break;
      case Req::PERCENT:
        PageLocs::send(
            {.req = PageLocs::Req::PERCENT, .itemrefIndex = static_cast<int16_t>(percentDone())});
      }
  }
}
