#include "page_locs_control.hpp"

#include "models/page_locs.hpp"

#include <chrono>

namespace {
constexpr const char *TAG          = "PageLocsControl";
constexpr int kCriticalSendRetries = 3;

auto sendToMgrChecked(const PageLocs::QueueData &msg, const char *ctx, bool critical = false)
    -> bool {
  const int attempts = critical ? kCriticalSendRetries : 1;
  for (int i = 0; i < attempts; ++i) {
    if (PageLocs::send(msg) >= 0) return true;
    pageLocs.telemetry.queueSendFailures.fetch_add(1);
    LOG_W("%s: PageLocs::send failed (attempt %d/%d)", ctx, i + 1, attempts);
  }
  if (critical) {
    LOG_E("%s: critical manager send failed after retries", ctx);
  }
  return false;
}

auto sendToRetrieverChecked(const PageLocsRetriever::QueueData &msg, const char *ctx,
                            bool critical = false) -> bool {
  const int attempts = critical ? kCriticalSendRetries : 1;
  for (int i = 0; i < attempts; ++i) {
    if (PageLocsRetriever::send(msg) >= 0) return true;
    pageLocs.telemetry.queueSendFailures.fetch_add(1);
    LOG_W("%s: PageLocsRetriever::send failed (attempt %d/%d)", ctx, i + 1, attempts);
  }
  if (critical) {
    LOG_E("%s: critical retriever send failed after retries", ctx);
  }
  return false;
}
} // namespace

#if EPUB_INKPLATE_BUILD
QueueHandle_t PageLocsControl::managerQueue{nullptr};
QueueHandle_t PageLocsControl::retrieverQueue{nullptr};
#else
mqd_t PageLocsControl::managerQueue{-1};
mqd_t PageLocsControl::retrieverQueue{-1};
#endif

/**
 * Initialize control-side queueing and start the control worker thread.
 *
 * Returns false if either retriever setup or control queue/thread setup fails.
 */
auto PageLocsControl::setup(const HimemString &epubFilename) -> bool {

  if (!retrieverTask.setup(epubFilename)) {
    LOG_E("Unable to setup retriever task");
    return false;
  }

#if EPUB_LINUX_BUILD
  mq_unlink("/control_mgr");
  mq_unlink("/control_r2c");

  managerQueue = mq_open("/control_mgr", O_RDWR | O_CREAT | O_NONBLOCK, S_IRWXU, &queueAttr);
  if (managerQueue == -1) {
    LOG_E("Unable to open managerQueue: %d", errno);
    return false;
  }

  retrieverQueue = mq_open("/control_r2c", O_RDWR | O_CREAT | O_NONBLOCK, S_IRWXU, &queueAttr);
  if (retrieverQueue == -1) {
    LOG_E("Unable to open retrieverQueue: %d", errno);
    return false;
  }

  controlThread = std::thread(&PageLocsControl::task, this);
#else

  if (managerQueue == nullptr) {
    managerQueue = xQueueCreate(5, sizeof(PageLocsControl::QueueData));
  } else {
    xQueueReset(managerQueue);
  }
  if (managerQueue == nullptr) {
    LOG_E("Unable to create managerQueue");
    return false;
  }

  if (retrieverQueue == nullptr) {
    retrieverQueue = xQueueCreate(5, sizeof(PageLocsControl::QueueData));
  } else {
    xQueueReset(retrieverQueue);
  }
  if (retrieverQueue == nullptr) {
    LOG_E("Unable to create retrieverQueue");
    return false;
  }

  if (pdPASS != xTaskCreatePinnedToCore([](void *param) {
    auto *self = static_cast<PageLocsControl *>(param);
    self->task();
    vTaskDelete(nullptr);
  }, "controlTask", 10 * 1024, this, (configMAX_PRIORITIES - 2) | portPRIVILEGE_BIT, nullptr, 1)) {
    LOG_E("Unable to create control task");
    return false;
  }

  controlTaskHandle = xTaskGetHandle("controlTask");

#endif

  return true;
}

/**
 * Join the control worker thread during shutdown.
 */
auto PageLocsControl::waitForExit() -> void {
#if EPUB_LINUX_BUILD
  if (controlThread.joinable()) {
    controlThread.join();
  }
#else
  // Signal the control task to stop and wait for it to exit before returning.
  while (eTaskGetState(controlTaskHandle) != eDeleted) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
#endif
}

auto PageLocsControl::sendPendingAsapReplies(int16_t itemref, const char *ctx,
                                             bool alreadySentToMgr) -> void {
  if (asapCorrelationIds.empty()) return;

  for (uint32_t correlationId : asapCorrelationIds) {
    if (!alreadySentToMgr || (correlationId != asapCorrelationIds.front())) {
      sendToMgrChecked({.req           = PageLocs::Req::ASAP_READY,
                        .correlationId = correlationId,
                        .itemrefIndex  = itemref},
                       ctx, true);
    }
  }
  asapCorrelationIds.clear();
}

/**
 * Select and dispatch the next retrieval work item.
 *
 * Priority is ASAP item, then deferred interrupted item, then next sequential
 * not-yet-computed item.
 */
auto PageLocsControl::requestNextItem(int16_t itemref, bool alreadySentToMgr) -> void {
  if (asapItemref != -1) { // is there an urgent spine to do?
    if (itemref == asapItemref) {
      asapItemref = -1;
      // if (!alreadySentToMgr) SHOW_IT("Sending ASAP_READY to Mgr");
      sendPendingAsapReplies(itemref, "requestNextItem/ASAP_READY", alreadySentToMgr);
    } else {
      waitingForItemref = asapItemref;
      asapItemref       = -1;
      SHOW_IT("GET_ASAP (%d) ----> ", waitingForItemref);
      sendToRetrieverChecked(
          {.req           = PageLocsRetriever::Req::GET_ASAP,
           .itemrefIndex  = waitingForItemref,
           .correlationId = asapCorrelationIds.empty() ? 0 : asapCorrelationIds.front()},
          "requestNextItem/GET_ASAP");
      return;
    }
  }
  if (nextItemrefToGet != -1) {
    waitingForItemref = nextItemrefToGet;
    nextItemrefToGet  = -1;
    SHOW_IT("RETRIEVE_ITEM (%d) ----> ", waitingForItemref);
    sendToRetrieverChecked({.req           = PageLocsRetriever::Req::RETRIEVE_ITEM,
                            .itemrefIndex  = waitingForItemref,
                            .correlationId = 0},
                           "requestNextItem/RETRIEVE_ITEM");
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
      SHOW_IT("RETRIEVE_ITEM itemref %d ---->", waitingForItemref);
      sendToRetrieverChecked({.req           = PageLocsRetriever::Req::RETRIEVE_ITEM,
                              .itemrefIndex  = waitingForItemref,
                              .correlationId = 0},
                             "requestNextItem/RETRIEVE_ITEM_NEXT");
    } else {
      SHOW_IT("COMPLETED ->");
      sendToRetrieverChecked({.req = PageLocsRetriever::Req::COMPLETED},
                             "requestNextItem/COMPLETED");
      pageLocs.computationCompleted();
      runningDown = true;
    }
  }
}

/**
 * Main control loop.
 *
 * Consumes manager/retriever messages, orchestrates scheduling, forwards
 * notifications to PageLocs, and coordinates retriever lifecycle.
 */
auto PageLocsControl::task() -> void {
  for (;;) {
    QueueData controlQueueData;

    bool hasMessage = false;

// Manager commands first so GET_ASAP/STOP are not delayed by retriever chatter.
#if EPUB_LINUX_BUILD
    if (receiveFromManager(controlQueueData, 0) >= 0) {
      hasMessage = true;
    } else if (receiveFromRetriever(controlQueueData, 0) >= 0) {
      hasMessage = true;
    }
#else
    if (receiveFromManager(controlQueueData, 0) == pdTRUE) {
      hasMessage = true;
    } else if (receiveFromRetriever(controlQueueData, 0) == pdTRUE) {
      hasMessage = true;
    }
#endif

    if (!hasMessage) {
#if EPUB_LINUX_BUILD
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
#else
      vTaskDelay(pdMS_TO_TICKS(1));
#endif
      continue;
    }

    switch (controlQueueData.req) {
    case Req::NONE:
      break;

    case Req::STOP:
      runningDown = true;
      retrieverTask.requestStop();
      SHOW_IT("STOP ->");
      sendToRetrieverChecked({.req = PageLocsRetriever::Req::STOP}, "task/STOP->retriever", true);

      retrieverTask.waitForExit();

      // SHOW_IT("Sending STOPPED to Mgr");
      sendToMgrChecked({.req = PageLocs::Req::STOPPED}, "task/STOPPED->mgr", true);
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
          SHOW_IT("RETRIEVE_ITEM itemref %d ---->", waitingForItemref);
          sendToRetrieverChecked({.req           = PageLocsRetriever::Req::RETRIEVE_ITEM,
                                  .itemrefIndex  = waitingForItemref,
                                  .correlationId = 0},
                                 "task/START_DOCUMENT/RETRIEVE_ITEM");
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
          // SHOW_IT("Sending ASAP_READY to Mgr for itemref %d", -(controlQueueData.itemrefIndex +
          // 1));
          sendToMgrChecked(
              {.req           = PageLocs::Req::ASAP_READY,
               .correlationId = controlQueueData.correlationId,
               .itemrefIndex  = static_cast<int16_t>(-(controlQueueData.itemrefIndex + 1))},
              "task/GET_ASAP/not_started", true);
        } else {
          int16_t itemref = controlQueueData.itemrefIndex;
          if ((itemref < 0) || (itemref >= itemrefCount)) {
            LOG_W("Ignoring invalid GET_ASAP item index %d (count=%d)", itemref, itemrefCount);
            sendToMgrChecked(
                {.req           = PageLocs::Req::ASAP_READY,
                 .correlationId = controlQueueData.correlationId,
                 .itemrefIndex  = (itemref >= 0) ? static_cast<int16_t>(-(itemref + 1)) : itemref},
                "task/GET_ASAP/invalid", true);
            break;
          }
          if ((bitset[itemref >> 3] & (1 << (itemref & 7))) != 0) {
            // SHOW_IT("Sending ASAP_READY to Mgr for itemref %d", itemref);
            sendToMgrChecked({.req           = PageLocs::Req::ASAP_READY,
                              .correlationId = controlQueueData.correlationId,
                              .itemrefIndex  = itemref},
                             "task/GET_ASAP/already_done", true);
          } else if (waitingForItemref != -1) {
            if (itemref != waitingForItemref) {
              // Replacing a previously pending ASAP target can orphan its
              // waiter correlations; complete them immediately with a miss so
              // callers do not block until timeout.
              if ((asapItemref != -1) && (asapItemref != itemref) && !asapCorrelationIds.empty()) {
                int16_t replacedItem = asapItemref;
                // SHOW_IT("Replacing pending ASAP item %d with %d (%zu waiters)", replacedItem,
                //         itemref, asapCorrelationIds.size());
                sendPendingAsapReplies(static_cast<int16_t>(-(replacedItem + 1)),
                                       "task/GET_ASAP/replaced", false);
              }

              asapItemref = itemref;
              asapCorrelationIds.clear();
              asapCorrelationIds.push_back(controlQueueData.correlationId);
              // Queue the ASAP request immediately so the retriever can notice it at the next
              // page boundary and switch items without completing the current one.
              SHOW_IT("GET_ASAP (%d) while (%d) ---->", itemref, waitingForItemref);
              if (!sendToRetrieverChecked({.req           = PageLocsRetriever::Req::GET_ASAP,
                                           .itemrefIndex  = itemref,
                                           .correlationId = controlQueueData.correlationId},
                                          "task/GET_ASAP/preempt", true)) {
                LOG_E("Failed to queue GET_ASAP(preempt) for item %d", itemref);
                sendToMgrChecked({.req           = PageLocs::Req::ASAP_READY,
                                  .correlationId = controlQueueData.correlationId,
                                  .itemrefIndex  = static_cast<int16_t>(-(itemref + 1))},
                                 "task/GET_ASAP/preempt_failed", true);
                asapItemref = -1;
                asapCorrelationIds.clear();
              }
            } else if (asapItemref == -1) {
              // The retriever is already computing this very item sequentially.
              // Instead of ignoring the request (which would cause a 2 s timeout),
              // mark asapItemref so that ASAP_READY is delivered to the manager
              // as soon as ITEM_READY arrives — no retriever interrupt needed.
              asapItemref = itemref;
              // Keep any waiter already queued for this same item (e.g. an earlier
              // GET_ASAP/idle request) and add this requester as an additional waiter.
              asapCorrelationIds.push_back(controlQueueData.correlationId);
              // SHOW_IT(
              // "GET_ASAP for current item %d: will send ASAP_READY on completion (%zu waiters)",
              // itemref, asapCorrelationIds.size());
            } else {
              // Coalesce another waiter for the same item; broadcast the eventual reply.
              asapCorrelationIds.push_back(controlQueueData.correlationId);
              // SHOW_IT("GET_ASAP for current item %d adds pending waiter (%zu total)", itemref,
              //         asapCorrelationIds.size());
            }
          } else {
            asapItemref = -1;
            asapCorrelationIds.clear();
            asapCorrelationIds.push_back(controlQueueData.correlationId);
            waitingForItemref = itemref;
            SHOW_IT("GET_ASAP itemref %d ->", itemref);
            if (!sendToRetrieverChecked({.req           = PageLocsRetriever::Req::GET_ASAP,
                                         .itemrefIndex  = itemref,
                                         .correlationId = controlQueueData.correlationId},
                                        "task/GET_ASAP/idle", true)) {
              LOG_E("Failed to queue GET_ASAP(idle) for item %d", itemref);
              waitingForItemref = -1;
              asapCorrelationIds.clear();
              sendToMgrChecked({.req           = PageLocs::Req::ASAP_READY,
                                .correlationId = controlQueueData.correlationId,
                                .itemrefIndex  = static_cast<int16_t>(-(itemref + 1))},
                               "task/GET_ASAP/idle_failed", true);
            }
          }
        }
      }
      break;

    // This is sent by the retrieval task, indicating that an item has been
    // processed.
    case Req::ITEM_READY:
      if (!runningDown) {
        SHOW_IT("ITEM_READY (%d) <----", controlQueueData.itemrefIndex);
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
        SHOW_IT("ASAP_READY (%d) <----", controlQueueData.itemrefIndex);
        waitingForItemref = -1;
        if (itemrefCount != -1) {
          int16_t itemref = controlQueueData.itemrefIndex;
          if (asapCorrelationIds.empty())
            asapCorrelationIds.push_back(controlQueueData.correlationId);
          // SHOW_IT("Sending ASAP_READY to Mgr for itemref %d (%zu waiters)", itemref,
          //         asapCorrelationIds.size());
          sendPendingAsapReplies(itemref, "task/ASAP_READY->mgr", false);
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
      // SHOW_IT("Sending PERCENT to MGR");
      sendToMgrChecked(
          {.req = PageLocs::Req::PERCENT, .itemrefIndex = static_cast<int16_t>(percentDone())},
          "task/PERCENT->mgr");
      break;

    // Sent by the retrieval task when it was aborted mid-item by a signalAbort() call.
    // The partially-computed pages are already in pagesMap (insert is idempotent), so the
    // item can safely be re-queued and reprocessed from scratch after the ASAP item is done.
    case Req::ITEM_INTERRUPTED:
      SHOW_IT("ITEM_INTERRUPTED (%d) <----", controlQueueData.itemrefIndex);
      if (!runningDown) {
        int16_t itemref = controlQueueData.itemrefIndex;
        // If this was an ASAP request (has non-zero correlationId), notify the manager
        // with a miss reply so it doesn't block indefinitely. The item will be re-queued
        // and computed later, but not as ASAP.
        if (controlQueueData.correlationId != 0) {
          sendToMgrChecked({.req           = PageLocs::Req::ASAP_READY,
                            .correlationId = controlQueueData.correlationId,
                            .itemrefIndex  = static_cast<int16_t>(-(itemref + 1))},
                           "task/ITEM_INTERRUPTED->mgr", true);
        }
        // SHOW_IT("Item %d interrupted; re-queuing after ASAP", itemref);
        waitingForItemref = -1;
        // Forward-bias heuristic: if the pending ASAP target is ahead of the
        // interrupted item, do not re-queue the interrupted item. Continue
        // from the ASAP item onward to match likely forward reading behavior.
        bool skipInterruptedRequeue =
            (asapItemref != -1) && (itemrefCount > 0) && (asapItemref > itemref);
        if (skipInterruptedRequeue) {
          nextItemrefToGet = -1;
          SHOW_IT("Skipping interrupted item %d after forward ASAP to %d", itemref, asapItemref);
        } else {
          // Re-queue interrupted work when no forward-ASAP preference applies.
          nextItemrefToGet = itemref;
        }

        // If there's a pending ASAP request, resend it to retriever since the original
        // GET_ASAP was consumed by the retriever's page boundary handler.
        if (asapItemref != -1) {
          waitingForItemref = asapItemref;
          SHOW_IT("GET_ASAP (%d) resend after interrupt ----> ", asapItemref);
          if (!sendToRetrieverChecked(
                  {.req           = PageLocsRetriever::Req::GET_ASAP,
                   .itemrefIndex  = asapItemref,
                   .correlationId = asapCorrelationIds.empty() ? 0 : asapCorrelationIds.front()},
                  "task/ITEM_INTERRUPTED/resend_GET_ASAP", true)) {
            LOG_E("Failed to resend GET_ASAP(%d) after interrupt", asapItemref);
            waitingForItemref = -1;
            asapItemref       = -1;
            asapCorrelationIds.clear();
          }
        } else {
          requestNextItem(-1);
        }
      }
      break;
    }
  }
}
