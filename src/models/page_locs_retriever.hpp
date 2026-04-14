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
  ~PageLocsRetriever() = default;

  enum class Req : int8_t {
    NONE, ABORT, RETRIEVE_ITEM, GET_ASAP, SHOW_HEAP
  };

  struct QueueData {
    Req req{Req::NONE};
    int16_t itemrefIndex{0};
  };

  static inline auto send(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      return mq_send(retrieverQueue, (const char *)&data, sizeof(data), 1);
    #else
      return xQueueSendToBack(retrieverQueue, &data, timeout);
    #endif
  }

  auto setup(const std::string &epubFilename) -> bool;

  auto waitForExit() -> void;

private:
  DOMPtr dom{nullptr};
  EPubPtr epub{nullptr};
  uint16_t pageBottom;

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

  auto buildPageLocs(int16_t itemrefIndex) -> bool;
};
