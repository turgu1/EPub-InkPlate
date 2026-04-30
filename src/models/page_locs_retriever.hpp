#pragma once

#include "global.hpp"

#include "models/dom.hpp"
#include "models/epub.hpp"
#include "viewers/html_interpreter.hpp"
#include "viewers/page.hpp"

#if EPUB_INKPLATE_BUILD
  #include <esp_pthread.h>
#else
  #include <mqueue.h>
#endif

#include <thread>

class PageLocsRetriever {
private:
  static constexpr const char *TAG = "PageLocsRetriever";

public:
  PageLocsRetriever()  = default;
  ~PageLocsRetriever() = default; // { LOG_I("PageLocsRetriever destructor called"); }

  enum class Req : int8_t {
    NONE, STOP, RETRIEVE_ITEM, GET_ASAP, SHOW_HEAP, COMPLETED
  };

  struct QueueData {
    Req req{Req::NONE};
    uint8_t reserved{0};
    int16_t itemrefIndex{0};
  };

  static inline auto send(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      (void)timeout;
      QueueData wireData{};
      wireData.req          = data.req;
      wireData.reserved     = 0;
      wireData.itemrefIndex = data.itemrefIndex;
      return mq_send(retrieverQueue, (const char *)&wireData, sizeof(wireData), 1);
    #else
      return xQueueSendToBack(retrieverQueue, &data, timeout);
    #endif
  }

  auto setup(const HimemString &epubFilename) -> bool;
  auto waitForExit() -> void;

  [[nodiscard]] inline auto getCurrentItemrefIndex() const { return currentItemrefIndex; }

private:
  DOMPtr dom{nullptr};
  EPubPtr epub{nullptr};
  uint16_t pageBottom{0};
  int16_t currentItemrefIndex{-1};

  EPub::BookFormatParams currentFormatParams;

  #if EPUB_LINUX_BUILD
    static mqd_t retrieverQueue;
    static constexpr mq_attr retrieverAttr = {0, 5, sizeof(QueueData), 0};
  #else
    static QueueHandle_t retrieverQueue;
  #endif

  auto receive(QueueData &data, int timeout = -1) {
    #if EPUB_LINUX_BUILD
      return mq_receive(retrieverQueue, (char *)&data, sizeof(data), nullptr);
    #else
      return xQueueReceive(retrieverQueue, &data, timeout);
    #endif
  }

  std::thread retrieverThread;
  auto task() -> void;

  auto retrieve_item(Req req, int16_t itemrefIndex) -> void;

  auto buildPageLocs(int16_t itemrefIndex) -> bool;
};
