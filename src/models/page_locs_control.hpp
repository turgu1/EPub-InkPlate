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

class PageLocsControl;

using PageLocsControlPtr = himemUniquePtr<PageLocsControl>;
class PageLocsControl {
private:
  PageLocsControl() = default;

public:
  ~PageLocsControl() { LOG_I(TAG, "PageLocsControl destructor called"); };

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> himemUniquePtr<T>;

  static inline auto Make() { return makeUniqueHimem<PageLocsControl>(); }

  enum class Req : int8_t {
    NONE, STOP, START_DOCUMENT, GET_ASAP, ITEM_READY, ASAP_READY, PERCENT
  };

  struct QueueData {
    Req req{Req::NONE};
    int16_t itemrefIndex{0};
    int16_t itemrefCount{0};
  };

  static inline auto send(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      return mq_send(controlQueue, (const char *)&data, sizeof(data), 1);
    #else
      return xQueueSendToBack(controlQueue, &data, timeout);
    #endif
  }

  auto setup(const std::string &epubFilename) -> bool;
  auto waitForExit() -> void;

  [[nodiscard]] inline auto percentDone() -> int {
    int16_t percent = (itemsDoneCount * 100) / itemrefCount;
    // ESP_LOGI(TAG, "Percent = %" PRIi16 " itemref count = %" PRIi16 " items done = %" PRIi16,
    // percent, itemrefCount, itemsDoneCount);
    return percent;
  }

private:
  static constexpr const char *TAG = "PageLocsControl";

  std::atomic<bool> runningDown{false};

  int16_t itemrefCount{-1};                  // Number of items in the document
  int16_t itemsDoneCount{-1};                // Number of items done
  int16_t waitingForItemref{-1};             // Current item being processed by retrieval task
  int16_t nextItemrefToGet{-1};              // Non prioritize item to get next
  int16_t asapItemref{-1};                   // Prioritize item to get next
  himemUniquePtr<uint8_t[]> bitset{nullptr}; // Set of all items processed so far
  uint8_t bitsetSize{0};                     // bitset byte length

  PageLocsRetriever retrieverTask;

  #if EPUB_LINUX_BUILD
    static mqd_t controlQueue;
    static constexpr mq_attr controlAttr = {0, 5, sizeof(QueueData), 0};
  #else
    static QueueHandle_t controlQueue;
  #endif

  auto receive(QueueData &data, int timeout = -1) {
    #if EPUB_LINUX_BUILD
      return mq_receive(controlQueue, (char *)&data, sizeof(data), nullptr);
    #else
      return xQueueReceive(controlQueue, &data, timeout);
    #endif
  }

  /**
   * Pick and dispatch the next retrieval request for the page-location pipeline.
   *
   * Selection priority:
   * 1) Pending ASAP request (asapItemref)
   * 2) Deferred normal request (nextItemrefToGet)
   * 3) Next not-yet-processed spine item found in bitset order
   *
   * Side effects:
   * - Sends requests to PageLocsRetriever (GET_ASAP, RETRIEVE_ITEM, or COMPLETED)
   * - Optionally notifies PageLocs manager with ASAP_READY
   * - Updates waitingForItemref, asapItemref, and nextItemrefToGet
   *
   * @param itemref Last itemref that just completed. Use -1 when no previous item is known.
   * @param alreadySentToMgr True when ASAP_READY for itemref was already sent by caller,
   *                         avoiding duplicate manager notification.
   */
  auto requestNextItem(int16_t itemref, bool alreadySentToMgr = false) -> void;

  std::thread controlThread;
  auto task() -> void;
};
