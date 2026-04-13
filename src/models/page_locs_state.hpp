#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/page_locs_retriever.hpp"

#if EPUB_INKPLATE_BUILD
  #include <esp_pthread.h>
#else
  #include <mqueue.h>
#endif

#include <thread>

class PageLocsState;

using PageLocsStatePtr = himem_unique_ptr<PageLocsState>;
class PageLocsState {
private:
  PageLocsState() = default;

public:
  ~PageLocsState() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make() { return make_unique_himem<PageLocsState>(); }

  enum class Req : int8_t {
    NONE, ABORT, STOP, START_DOCUMENT, GET_ASAP, ITEM_READY, ASAP_READY, PERCENT
  };

  struct QueueData {
    Req req{Req::NONE};
    int16_t itemref_index{0};
    int16_t itemref_count{0};
  };

  static inline auto send(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      return mq_send(state_queue, (const char *)&data, sizeof(data), 1);
    #else
      return xQueueSendToBack(state_queue, &data, timeout);
    #endif
  }

  auto setup(const std::string &epub_filename) -> bool;
  void wait_for_exit();

  inline bool retriever_is_iddle() { return retriever_iddle; }
  inline bool forgetting_retrieval() { return forget_retrieval; }

  inline int percent_done() {
    int16_t percent = (items_done_count * 100) / itemref_count;
    // ESP_LOGI(TAG, "Percent = %" PRIi16 " itemref count = %" PRIi16 " items done = %" PRIi16,
    // percent, itemref_count, items_done_count);
    return percent;
  }

private:
  static constexpr const char *TAG = "PageLocsState";

  bool retriever_iddle{true};

  int16_t itemref_count{-1};       // Number of items in the document
  int16_t items_done_count{-1};    // Number of items done
  int16_t waiting_for_itemref{-1}; // Current item being processed by retrieval task
  int16_t next_itemref_to_get{-1}; // Non prioritize item to get next
  int16_t asap_itemref{-1};        // Prioritize item to get next
  uint8_t *bitset{nullptr};        // Set of all items processed so far
  uint8_t bitset_size{0};          // bitset byte length
  bool stopping{false};
  bool forget_retrieval{false}; // Forget current item begin processed by retrieval task

  PageLocsRetriever retriever_task;

  #if EPUB_LINUX_BUILD
    static mqd_t state_queue;
    static constexpr mq_attr state_attr = {0, 5, sizeof(QueueData), 0};
  #else
    static QueueHandle_t state_queue;
  #endif

  auto receive(QueueData &data, int timeout = -1) {
    #if EPUB_LINUX_BUILD
      return mq_receive(state_queue, (char *)&data, sizeof(data), nullptr);
    #else
      return xQueueReceive(state_queue, &data, timeout);
    #endif
  }

  void abort();

  void request_next_item(int16_t itemref, bool already_sent_to_mgr);

  std::thread state_thread;
  void task();
};
