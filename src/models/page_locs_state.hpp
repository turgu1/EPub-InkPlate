#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/page_locs_retriever.hpp"

#if EPUB_INKPLATE_BUILD
  #include <esp_pthread.h>
#else
  #include <mqueue.h>
#endif

#include <atomic>
#include <thread>

class PageLocsState;

using PageLocsStatePtr = himemUniquePtr<PageLocsState>;
class PageLocsState {
private:
  PageLocsState() = default;

public:
  ~PageLocsState() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> himemUniquePtr<T>;

  static inline auto Make() { return makeUniqueHimem<PageLocsState>(); }

  enum class Req : int8_t {
    NONE, ABORT, STOP, START_DOCUMENT, GET_ASAP, ITEM_READY, ASAP_READY, PERCENT
  };

  struct QueueData {
    Req req{Req::NONE};
    int16_t itemrefIndex{0};
    int16_t itemrefCount{0};
  };

  static inline auto send(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      return mq_send(stateQueue, (const char *)&data, sizeof(data), 1);
    #else
      return xQueueSendToBack(stateQueue, &data, timeout);
    #endif
  }

  auto setup(const std::string &epubFilename) -> bool;
  void waitForExit();

  inline auto retrieverIsIdle() -> bool { return retrieverIdle; }
  inline auto forgettingRetrieval() -> bool { return forgetRetrieval; }

  inline auto percentDone() -> int {
    int16_t percent = (itemsDoneCount * 100) / itemrefCount;
    // ESP_LOGI(TAG, "Percent = %" PRIi16 " itemref count = %" PRIi16 " items done = %" PRIi16,
    // percent, itemrefCount, itemsDoneCount);
    return percent;
  }

private:
  static constexpr const char *TAG = "PageLocsState";

  std::atomic<bool> retrieverIdle{true};

  int16_t itemrefCount{-1};      // Number of items in the document
  int16_t itemsDoneCount{-1};    // Number of items done
  int16_t waitingForItemref{-1}; // Current item being processed by retrieval task
  int16_t nextItemrefToGet{-1};  // Non prioritize item to get next
  int16_t asapItemref{-1};       // Prioritize item to get next
  uint8_t *bitset{nullptr};      // Set of all items processed so far
  uint8_t bitsetSize{0};         // bitset byte length
  bool stopping{false};
  std::atomic<bool> forgetRetrieval{false}; // Forget current item being processed by retrieval task

  PageLocsRetriever retrieverTask;

  #if EPUB_LINUX_BUILD
    static mqd_t stateQueue;
    static constexpr mq_attr stateAttr = {0, 5, sizeof(QueueData), 0};
  #else
    static QueueHandle_t stateQueue;
  #endif

  auto receive(QueueData &data, int timeout = -1) {
    #if EPUB_LINUX_BUILD
      return mq_receive(stateQueue, (char *)&data, sizeof(data), nullptr);
    #else
      return xQueueReceive(stateQueue, &data, timeout);
    #endif
  }

  void abort();

  void requestNextItem(int16_t itemref, bool alreadySentToMgr = false);

  std::thread stateThread;
  void task();
};
