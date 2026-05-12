#include "page_locs_control.hpp"

#include "models/page_locs.hpp"

#if EPUB_INKPLATE_BUILD
  QueueHandle_t PageLocsControl::controlQueue{nullptr};
#else
  mqd_t PageLocsControl::controlQueue{-1};
#endif

auto PageLocsControl::setup(const HimemString &epubFilename) -> bool {

  if (!retrieverTask.setup(epubFilename)) {
    LOG_E("Unable to setup retriever task");
    return false;
  }

  #if EPUB_LINUX_BUILD
    mq_unlink("/control");

    controlQueue = mq_open("/control", O_RDWR | O_CREAT, S_IRWXU, &controlAttr);
    if (controlQueue == -1) {
      LOG_E("Unable to open controlQueue: %d", errno);
      return false;
    }

    controlThread = std::thread(&PageLocsControl::task, this);
  #else
    esp_pthread_init();

    if (controlQueue == nullptr) {
      controlQueue = xQueueCreate(5, sizeof(PageLocsControl::QueueData));
    } else {
      xQueueReset(controlQueue);
    }
    if (controlQueue == nullptr) {
      LOG_E("Unable to create controlQueue");
      return false;
    }

    auto cfg        = esp_pthread_get_default_config();
    cfg.thread_name = "controlTask";
    cfg.pin_to_core = 0;
    cfg.stack_size  = 10 * 1024;
    cfg.prio        = configMAX_PRIORITIES - 2;
    cfg.inherit_cfg = true;

    LOG_D("Control task cfg: name=%s core=%d stack=%u prio=%d inherit=%d",
          (cfg.thread_name != nullptr) ? cfg.thread_name : "(null)", cfg.pin_to_core,
          static_cast<unsigned>(cfg.stack_size), cfg.prio, cfg.inherit_cfg ? 1 : 0);

    esp_pthread_set_cfg(&cfg);
    controlThread = std::thread(&PageLocsControl::task, this);

  #endif

  return true;
}

auto PageLocsControl::waitForExit() -> void {
  if (controlThread.joinable()) {
    controlThread.join();
  }
}

auto PageLocsControl::requestNextItem(int16_t itemref, bool alreadySentToMgr) -> void {
  if (asapItemref != -1) { // is there an urgent spine to do?
    if (itemref == asapItemref) {
      asapItemref = -1;
      if (!alreadySentToMgr) {
        SHOW_IT("Sending ASAP_READY to Mgr");
        PageLocs::send({.req = PageLocs::Req::ASAP_READY, .itemrefIndex = itemref});
      }
    } else {
      waitingForItemref = asapItemref;
      asapItemref       = -1;
      SHOW_IT("Sending GET_ASAP to Retriever");
      PageLocsRetriever::send(
          {.req = PageLocsRetriever::Req::GET_ASAP, .itemrefIndex = waitingForItemref});
      return;
    }
  }
  if (nextItemrefToGet != -1) {
    waitingForItemref = nextItemrefToGet;
    nextItemrefToGet  = -1;
    SHOW_IT("Sending RETRIEVE_ITEM to Retriever");
    PageLocsRetriever::send(
        {.req = PageLocsRetriever::Req::RETRIEVE_ITEM, .itemrefIndex = waitingForItemref});
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
      SHOW_IT("Sending RETRIEVE_ITEM to Retriever for itemref %d", waitingForItemref);
      PageLocsRetriever::send(
          {.req = PageLocsRetriever::Req::RETRIEVE_ITEM, .itemrefIndex = waitingForItemref});
    } else {
      SHOW_IT("All items done. Sending COMPLETED to Retriever and Mgr...");
      PageLocsRetriever::send({.req = PageLocsRetriever::Req::COMPLETED});
      pageLocs.computationCompleted();
      runningDown = true;
    }
  }
}

auto PageLocsControl::task() -> void {
  for (;;) {
    LOG_D("==> Waiting for request... <==");
    QueueData controlQueueData;

    if (receive(controlQueueData) == -1) {
      LOG_E("Receive error: %d: %s", errno, strerror(errno));
    } else
      switch (controlQueueData.req) {
      case Req::NONE:
        break;

      case Req::STOP:
        runningDown = true;
        retrieverTask.requestStop();
        SHOW_IT("abortControlTask: Sending STOP to Retriever");
        PageLocsRetriever::send({.req = PageLocsRetriever::Req::STOP});

        retrieverTask.waitForExit();

        SHOW_IT("Sending STOPPED to Mgr");
        PageLocs::send({.req = PageLocs::Req::STOPPED});
        return;

      case Req::START_DOCUMENT:
        LOG_D("-> START_DOCUMENT <-");
        itemrefCount   = controlQueueData.itemrefCount;
        itemsDoneCount = 0;

        bitsetSize = (itemrefCount + 7) >> 3;
        bitset     = makeUniqueHimem<uint8_t[]>(bitsetSize);

        if (bitset) {
          memset(bitset.get(), 0, bitsetSize);
          if (waitingForItemref == -1) {
            waitingForItemref = controlQueueData.itemrefIndex;
            SHOW_IT("Sending RETRIEVE_ITEM to Retriever for itemref %d", waitingForItemref);
            PageLocsRetriever::send(
                {.req = PageLocsRetriever::Req::RETRIEVE_ITEM, .itemrefIndex = waitingForItemref});
          } else {
            nextItemrefToGet = controlQueueData.itemrefIndex;
          }
        } else {
          LOG_E("Unable to allocate bitset for %d items", itemrefCount);
          pageLocs.computationAborted("Unable to allocate bitset");
          itemrefCount = -1;
          runningDown  = true;
        }
        break;

      case Req::GET_ASAP:
        if (!runningDown) {
          LOG_D("-> GET_ASAP <-");
          // Mgr request a specific item. If document retrieval not started,
          // return a negative value.
          // If already done, let it know it a.s.a.p. If currently being processed,
          // keep a mark when it will be back. If not, queue the request.
          if (itemrefCount == -1) {
            SHOW_IT("Sending ASAP_READY to Mgr for itemref %d",
                    -(controlQueueData.itemrefIndex + 1));
            PageLocs::send(
                {.req          = PageLocs::Req::ASAP_READY,
                 .itemrefIndex = static_cast<int16_t>(-(controlQueueData.itemrefIndex + 1))});
          } else {
            int16_t itemref = controlQueueData.itemrefIndex;
            if ((itemref < 0) || (itemref >= itemrefCount)) {
              LOG_W("Ignoring invalid GET_ASAP item index %d (count=%d)", itemref, itemrefCount);
              PageLocs::send({.req = PageLocs::Req::ASAP_READY,
                              .itemrefIndex =
                                  (itemref >= 0) ? static_cast<int16_t>(-(itemref + 1)) : itemref});
              break;
            }
            if ((bitset[itemref >> 3] & (1 << (itemref & 7))) != 0) {
              SHOW_IT("Sending ASAP_READY to Mgr for itemref %d", itemref);
              PageLocs::send({.req = PageLocs::Req::ASAP_READY, .itemrefIndex = itemref});
            } else if (waitingForItemref != -1) {
              if (itemref != waitingForItemref) {
                asapItemref = itemref;
                // Queue the ASAP request immediately so the retriever can notice it at the next
                // page boundary and switch items without completing the current one.
                SHOW_IT("Queueing GET_ASAP to Retriever for itemref %d while item %d is running",
                        itemref, waitingForItemref);
                PageLocsRetriever::send(
                    {.req = PageLocsRetriever::Req::GET_ASAP, .itemrefIndex = itemref});
              } else if (asapItemref == -1) {
                // The retriever is already computing this very item sequentially.
                // Instead of ignoring the request (which would cause a 2 s timeout),
                // mark asapItemref so that ASAP_READY is delivered to the manager
                // as soon as ITEM_READY arrives — no retriever interrupt needed.
                asapItemref = itemref;
                SHOW_IT("GET_ASAP for current item %d: will send ASAP_READY on completion",
                        itemref);
              } else {
                SHOW_IT("GET_ASAP for current item %d ignored (asapItemref=%d already pending)",
                        itemref, asapItemref);
              }
            } else {
              asapItemref       = -1;
              waitingForItemref = itemref;
              SHOW_IT("Sending GET_ASAP to Retriever for itemref %d", itemref);
              PageLocsRetriever::send(
                  {.req = PageLocsRetriever::Req::GET_ASAP, .itemrefIndex = itemref});
            }
          }
        }
        break;

      // This is sent by the retrieval task, indicating that an item has been
      // processed.
      case Req::ITEM_READY:
        if (!runningDown) {
          LOG_D("-> ITEM_READY <-");
          waitingForItemref = -1;
          if (itemrefCount != -1) {
            int16_t itemref = -1;

            itemref = controlQueueData.itemrefIndex;
            if (itemref < 0) {
              itemref = -(itemref + 1);
              LOG_E("Unable to retrieve pages location for item %d", itemref);
            }
            if ((itemref < 0) || (itemref >= itemrefCount)) {
              LOG_E("Ignoring ITEM_READY with invalid item index %d (count=%d)", itemref,
                    itemrefCount);
              requestNextItem(-1);
              break;
            }
            if ((bitset[itemref >> 3] & (1 << (itemref & 7))) == 0) {
              bitset[itemref >> 3] |= (1 << (itemref & 7));
              itemsDoneCount += 1;
              // ESP_LOGI(TAG,"itemsDoneCount = %" PRIi16 " of %" PRIi16, itemsDoneCount,
              // itemrefCount);
            }

            requestNextItem(itemref);
          }
        }
        break;

      // This is sent by the retrieval task, indicating that an ASAP item has been
      // processed.
      case Req::ASAP_READY:
        if (!runningDown) {
          LOG_D("-> ASAP_READY <-");
          waitingForItemref = -1;
          if (itemrefCount != -1) {
            int16_t itemref = controlQueueData.itemrefIndex;
            SHOW_IT("Sending ASAP_READY to Mgr for itemref %d", itemref);
            PageLocs::send({.req = PageLocs::Req::ASAP_READY, .itemrefIndex = itemref});
            if (itemref < 0) {
              itemref = -(itemref + 1);
              LOG_E("Unable to retrieve pages location for item %d", itemref);
            }
            if ((itemref < 0) || (itemref >= itemrefCount)) {
              LOG_E("Ignoring ASAP_READY with invalid item index %d (count=%d)", itemref,
                    itemrefCount);
              requestNextItem(-1, true);
              break;
            }
            if ((bitset[itemref >> 3] & (1 << (itemref & 7))) == 0) {
              bitset[itemref >> 3] |= (1 << (itemref & 7));
              itemsDoneCount += 1;
              // ESP_LOGI(TAG,"itemsDoneCount = %" PRIi16" of %" PRIi16, itemsDoneCount,
              // itemrefCount);
            }

            requestNextItem(itemref, true);
          }
        }
        break;

      case Req::PERCENT:
        SHOW_IT("Sending PERCENT to MGR");
        PageLocs::send(
            {.req = PageLocs::Req::PERCENT, .itemrefIndex = static_cast<int16_t>(percentDone())});
        break;

      // Sent by the retrieval task when it was aborted mid-item by a signalAbort() call.
      // The partially-computed pages are already in pagesMap (insert is idempotent), so the
      // item can safely be re-queued and reprocessed from scratch after the ASAP item is done.
      case Req::ITEM_INTERRUPTED:
        if (!runningDown) {
          LOG_D("-> ITEM_INTERRUPTED <-");
          int16_t itemref = controlQueueData.itemrefIndex;
          SHOW_IT("Item %d interrupted; re-queuing after ASAP", itemref);
          waitingForItemref = -1;
          // Re-queue the interrupted item as a deferred request so it will be
          // retried after the pending ASAP item is served.
          nextItemrefToGet = itemref;
          if (asapItemref == -1) requestNextItem(-1);
        }
        break;
      }
  }
}
