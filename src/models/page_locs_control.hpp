#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/page_locs_retriever.hpp"

#if EPUB_INKPLATE_BUILD
  #include <esp_pthread.h>
#else
  #include <mqueue.h>
  #include <time.h>
#endif

#include <atomic>
#include <thread>
#include <vector>

class PageLocsControl;

using PageLocsControlPtr = HimemUniquePtr<PageLocsControl>;
class PageLocsControl {
private:
  static constexpr const char *TAG = "PageLocsControl";
  PageLocsControl()                = default;

public:
  ~PageLocsControl() = default; // { LOG_I("PageLocsControl destructor called"); };

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> HimemUniquePtr<T>;

  static inline auto Make() { return makeUniqueHimem<PageLocsControl>(); }

  enum class Req : int8_t {
    NONE, STOP, START_DOCUMENT, GET_ASAP, ITEM_READY, ASAP_READY, PERCENT, ITEM_INTERRUPTED
  };

  struct QueueData {
    Req req{Req::NONE};
    uint8_t reserved{0};
    int16_t itemrefIndex{0};
    int16_t itemrefCount{0};
    uint32_t correlationId{0}; // Hardening #1: correlation ID for tracking
  };

  static inline auto send(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      (void)timeout;
      QueueData wireData{};
      wireData.req           = data.req;
      wireData.reserved      = 0;
      wireData.itemrefIndex  = data.itemrefIndex;
      wireData.itemrefCount  = data.itemrefCount;
      wireData.correlationId = data.correlationId;
      return mq_send(managerQueue, (const char *)&wireData, sizeof(wireData), 1);
    #else
      return xQueueSendToBack(managerQueue, &data, timeout);
    #endif
  }

  // Retriever notifications use a dedicated queue to avoid starvation/mixing
  // with manager commands (GET_ASAP/STOP/PERCENT).
  static inline auto sendFromRetriever(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      (void)timeout;
      QueueData wireData{};
      wireData.req           = data.req;
      wireData.reserved      = 0;
      wireData.itemrefIndex  = data.itemrefIndex;
      wireData.itemrefCount  = data.itemrefCount;
      wireData.correlationId = data.correlationId;
      return mq_send(retrieverQueue, (const char *)&wireData, sizeof(wireData), 1);
    #else
      return xQueueSendToBack(retrieverQueue, &data, timeout);
    #endif
  }

  auto setup(const HimemString &epubFilename) -> bool;
  auto waitForExit() -> void;

  [[nodiscard]] inline auto percentDone() -> int {
    int16_t percent = (itemsDoneCount * 100) / itemrefCount;
    // ESP_LOGI(TAG, "Percent = %" PRIi16 " itemref count = %" PRIi16 " items done = %" PRIi16,
    // percent, itemrefCount, itemsDoneCount);
    return percent;
  }

  [[nodiscard]] inline auto getCurrentItemrefIndex() const {
    return retrieverTask.getCurrentItemrefIndex();
  }

private:
  std::atomic<bool> runningDown{false};

  int16_t itemrefCount{-1};                  // Number of items in the document
  int16_t itemsDoneCount{-1};                // Number of items done
  int16_t waitingForItemref{-1};             // Current item being processed by retrieval task
  int16_t nextItemrefToGet{-1};              // Non prioritize item to get next
  int16_t asapItemref{-1};                   // Prioritize item to get next
  std::vector<uint32_t> asapCorrelationIds;  // Correlations for pending ASAP manager replies
  HimemUniquePtr<uint8_t[]> bitset{nullptr}; // Set of all items processed so far
  uint8_t bitsetSize{0};                     // bitset byte length

  PageLocsRetriever retrieverTask;

  #if EPUB_LINUX_BUILD
    static mqd_t managerQueue;
    static mqd_t retrieverQueue;
    static constexpr mq_attr queueAttr = {0, 5, sizeof(QueueData), 0};
  #else
    static QueueHandle_t managerQueue;
    static QueueHandle_t retrieverQueue;
  #endif

  auto receiveFromManager(QueueData &data, int timeout = -1) {
    #if EPUB_LINUX_BUILD
      if (timeout <= 0) {
        return mq_receive(managerQueue, (char *)&data, sizeof(data), nullptr);
      }

      timespec ts{};
      clock_gettime(CLOCK_REALTIME, &ts);
      const int64_t nsecTotal =
          static_cast<int64_t>(ts.tv_nsec) + static_cast<int64_t>(timeout) * 1000000LL;
      ts.tv_sec += static_cast<time_t>(nsecTotal / 1000000000LL);
      ts.tv_nsec = static_cast<long>(nsecTotal % 1000000000LL);
      return mq_timedreceive(managerQueue, (char *)&data, sizeof(data), nullptr, &ts);
    #else
      return xQueueReceive(managerQueue, &data, timeout);
    #endif
  }

  auto receiveFromRetriever(QueueData &data, int timeout = -1) {
    #if EPUB_LINUX_BUILD
      if (timeout <= 0) {
        return mq_receive(retrieverQueue, (char *)&data, sizeof(data), nullptr);
      }

      timespec ts{};
      clock_gettime(CLOCK_REALTIME, &ts);
      const int64_t nsecTotal =
          static_cast<int64_t>(ts.tv_nsec) + static_cast<int64_t>(timeout) * 1000000LL;
      ts.tv_sec += static_cast<time_t>(nsecTotal / 1000000000LL);
      ts.tv_nsec = static_cast<long>(nsecTotal % 1000000000LL);
      return mq_timedreceive(retrieverQueue, (char *)&data, sizeof(data), nullptr, &ts);
    #else
      return xQueueReceive(retrieverQueue, &data, timeout);
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
  auto sendPendingAsapReplies(int16_t itemref, const char *ctx, bool alreadySentToMgr = false)
      -> void;

  std::thread controlThread;
  auto task() -> void;
};
