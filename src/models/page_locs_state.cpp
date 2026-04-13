#include "page_locs_state.hpp"

#include "models/page_locs.hpp"

#if EPUB_INKPLATE_BUILD
  QueueHandle_t PageLocsState::state_queue{nullptr};
#else
  mqd_t PageLocsState::state_queue{-1};
#endif

auto PageLocsState::setup(const std::string &epub_filename) -> bool {
  if (!retriever_task.setup(epub_filename)) {
    LOG_E("Unable to setup retriever task");
    return false;
  }

  #if EPUB_LINUX_BUILD
    mq_unlink("/state");

    state_queue = mq_open("/state", O_RDWR | O_CREAT, S_IRWXU, &state_attr);
    if (state_queue == -1) {
      LOG_E("Unable to open state_queue: %d", errno);
      return false;
    }

    state_thread = std::thread(&PageLocsState::task, this);
  #else
    esp_pthread_init();

    if (state_queue == nullptr) state_queue = xQueueCreate(5, sizeof(PageLocsState::QueueData));
    if (state_queue == nullptr) {
      LOG_E("Unable to create state_queue");
      return false;
    }

    auto cfg        = esp_pthread_get_default_config();
    cfg.thread_name = "stateTask";
    cfg.pin_to_core = 0;
    cfg.stack_size  = 10 * 1024;
    cfg.prio        = configMAX_PRIORITIES - 2;
    cfg.inherit_cfg = true;

    esp_pthread_set_cfg(&cfg);
    state_thread = std::thread(&PageLocsState::task, this);

  #endif

  return true;
}

void PageLocsState::wait_for_exit() {
  state_thread.join();
  state_thread.~thread();
}

void PageLocsState::abort() {
  LOG_D("abort_threads: Sending ABORT to Retriever");
  PageLocsRetriever::send({.req = PageLocsRetriever::Req::ABORT});

  retriever_task.wait_for_exit();
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
void PageLocsState::request_next_item(int16_t itemref, bool already_sent_to_mgr = false) {
  if (asap_itemref != -1) { // is there an urgent spine to do?
    if (itemref == asap_itemref) {
      asap_itemref = -1;
      if (!already_sent_to_mgr) {
        PageLocs::send({.req = PageLocs::Req::ASAP_READY, .itemref_index = itemref});
        LOG_D("Sent ASAP_READY to Mgr");
      }
    } else {
      waiting_for_itemref = asap_itemref;
      asap_itemref        = -1;
      PageLocsRetriever::send(
          {.req = PageLocsRetriever::Req::GET_ASAP, .itemref_index = waiting_for_itemref});
      retriever_iddle = false;
      LOG_D("Sent GET_ASAP to Retriever");
      return;
    }
  }
  if (next_itemref_to_get != -1) {
    waiting_for_itemref = next_itemref_to_get;
    next_itemref_to_get = -1;
    PageLocsRetriever::send(
        {.req = PageLocsRetriever::Req::RETRIEVE_ITEM, .itemref_index = waiting_for_itemref});
    retriever_iddle = false;
    LOG_D("Sent RETRIEVE_ITEM to Retriever");
  } else {
    int16_t newref;
    if (itemref != -1) {
      newref = (itemref + 1) % itemref_count;
    } else {
      itemref = 0;
      newref  = 0;
    }
    while ((bitset[newref >> 3] & (1 << (newref & 7))) != 0) {
      newref = (newref + 1) % itemref_count;
      if (newref == itemref) break;
    }
    if (newref != itemref) {
      waiting_for_itemref = newref;
      PageLocsRetriever::send(
          {.req = PageLocsRetriever::Req::RETRIEVE_ITEM, .itemref_index = waiting_for_itemref});
      retriever_iddle = false;
      LOG_D("Sent RETRIEVE_ITEM to Retriever");
    } else {
      page_locs.computation_completed();
      retriever_iddle = true;
    }
  }
}

void PageLocsState::task() {
  for (;;) {
    LOG_D("==> Waiting for request... <==");
    QueueData state_queue_data;

    if (receive(state_queue_data) == -1) {
      LOG_E("Receive error: %d: %s", errno, strerror(errno));
    } else
      switch (state_queue_data.req) {
      case Req::NONE:
        break;
      case Req::ABORT:
        abort();
        return;

      case Req::STOP:
        LOG_D("-> STOP <-");
        itemref_count    = -1;
        forget_retrieval = true;
        if (bitset != nullptr) {
          delete[] bitset;
          bitset = nullptr;
        }
        if (retriever_iddle) {
          PageLocs::send({.req = PageLocs::Req::STOPPED, .itemref_index = 0});
        } else {
          stopping = true;
        }
        break;

      case Req::START_DOCUMENT:
        LOG_D("-> START_DOCUMENT <-");
        if (bitset) delete[] bitset;
        itemref_count    = state_queue_data.itemref_count;
        items_done_count = 0;
        // ESP_LOGI(TAG,"items_done_count = %" PRIi16 " of %" PRIi16, items_done_count,
        // itemref_count);
        bitset_size = (itemref_count + 7) >> 3;
        bitset      = new uint8_t[bitset_size];
        if (bitset) {
          memset(bitset, 0, bitset_size);
          if (waiting_for_itemref == -1) {
            waiting_for_itemref = state_queue_data.itemref_index;
            forget_retrieval    = false;
            PageLocsRetriever::send({.req           = PageLocsRetriever::Req::RETRIEVE_ITEM,
                                     .itemref_index = waiting_for_itemref});
            LOG_D("Sent RETRIEVE_ITEM to retriever");
          } else {
            forget_retrieval    = true;
            next_itemref_to_get = state_queue_data.itemref_index;
          }
          retriever_iddle = false;
        } else {
          itemref_count    = -1;
          retriever_iddle  = true;
          forget_retrieval = true;
        }
        break;

      case Req::GET_ASAP:
        LOG_D("-> GET_ASAP <-");
        // Mgr request a specific item. If document retrieval not started,
        // return a negative value.
        // If already done, let it know it a.s.a.p. If currently being processed,
        // keep a mark when it will be back. If not, queue the request.
        if (itemref_count == -1) {
          PageLocs::send(
              {.req           = PageLocs::Req::ASAP_READY,
               .itemref_index = static_cast<int16_t>(-(state_queue_data.itemref_index + 1))});
          LOG_D("Sent ASAP_READY to Mgr");
        } else {
          int16_t itemref = state_queue_data.itemref_index;
          if ((bitset[itemref >> 3] & (1 << (itemref & 7))) != 0) {
            PageLocs::send({.req = PageLocs::Req::ASAP_READY, .itemref_index = itemref});
            LOG_D("Sent ASAP_READY to Mgr");
          } else if (waiting_for_itemref != -1) {
            asap_itemref = itemref;
          } else {
            asap_itemref        = -1;
            waiting_for_itemref = itemref;
            retriever_iddle     = false;
            PageLocsRetriever::send(
                {.req = PageLocsRetriever::Req::GET_ASAP, .itemref_index = itemref});
            LOG_D("Sent GET_ASAP to Retriever");
          }
        }
        break;

      // This is sent by the retrieval task, indicating that an item has been
      // processed.
      case Req::ITEM_READY:
        LOG_D("-> ITEM_READY <-");
        waiting_for_itemref = -1;
        if (itemref_count != -1) {
          int16_t itemref = -1;
          if (forget_retrieval) {
            forget_retrieval = false;
          } else {
            itemref = state_queue_data.itemref_index;
            if (itemref < 0) {
              itemref = -(itemref + 1);
              LOG_E("Unable to retrieve pages location for item %d", itemref);
            }
            if ((bitset[itemref >> 3] & (1 << (itemref & 7))) == 0) {
              bitset[itemref >> 3] |= (1 << (itemref & 7));
              items_done_count += 1;
              // ESP_LOGI(TAG,"items_done_count = %" PRIi16 " of %" PRIi16, items_done_count,
              // itemref_count);
            }
          }
          if (stopping) {
            stopping        = false;
            retriever_iddle = true;
            PageLocs::send({.req = PageLocs::Req::STOPPED});
          } else {
            request_next_item(itemref);
          }
        } else {
          if (stopping) {
            stopping        = false;
            retriever_iddle = true;
            PageLocs::send({.req = PageLocs::Req::STOPPED});
          }
        }
        break;

      // This is sent by the retrieval task, indicating that an ASAP item has been
      // processed.
      case Req::ASAP_READY:
        LOG_D("-> ASAP_READY <-");
        waiting_for_itemref = -1;
        if (itemref_count != -1) {
          int16_t itemref = state_queue_data.itemref_index;
          PageLocs::send({.req = PageLocs::Req::ASAP_READY, .itemref_index = itemref});
          LOG_D("Sent ASAP_READY to Mgr");
          if (itemref < 0) {
            itemref = -(itemref + 1);
            LOG_E("Unable to retrieve pages location for item %d", itemref);
          }
          if ((bitset[itemref >> 3] & (1 << (itemref & 7))) == 0) {
            bitset[itemref >> 3] |= (1 << (itemref & 7));
            items_done_count += 1;
            // ESP_LOGI(TAG,"items_done_count = %" PRIi16" of %" PRIi16, items_done_count,
            // itemref_count);
          }
          if (stopping) {
            stopping        = false;
            retriever_iddle = true;
            PageLocs::send({.req = PageLocs::Req::STOPPED, .itemref_index = 0});
          } else {
            request_next_item(itemref, true);
          }
        } else {
          if (stopping) {
            stopping        = false;
            retriever_iddle = true;
            PageLocs::send({.req = PageLocs::Req::STOPPED, .itemref_index = 0});
          }
        }
        break;
      case Req::PERCENT:
        PageLocs::send(
            {.req = PageLocs::Req::PERCENT, .itemref_index = static_cast<int16_t>(percent_done())});
      }
  }
}
