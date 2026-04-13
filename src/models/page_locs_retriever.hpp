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
    int16_t itemref_index{0};
  };

  static inline auto send(const QueueData data, int timeout = 0) {
    #if EPUB_LINUX_BUILD
      return mq_send(retriever_queue, (const char *)&data, sizeof(data), 1);
    #else
      return xQueueSendToBack(retriever_queue, &data, timeout);
    #endif
  }

  auto setup(const std::string &epub_filename) -> bool;

  void wait_for_exit();

private:
  DOMPtr dom{nullptr};
  EPubPtr epub{nullptr};
  uint16_t page_bottom;

  EPub::BookFormatParams current_format_params;

  #if EPUB_LINUX_BUILD
    static mqd_t retriever_queue;
    static constexpr mq_attr retriever_attr = {0, 5, sizeof(QueueData), 0};
  #else
    static QueueHandle_t retriever_queue;
  #endif

  auto receive(QueueData &data, int timeout = -1) {
    #if EPUB_LINUX_BUILD
      return mq_receive(retriever_queue, (char *)&data, sizeof(data), nullptr);
    #else
      return xQueueReceive(retriever_queue, &data, timeout);
    #endif
  }

  std::thread retriever_thread;
  void task();

  bool build_page_locs(int16_t itemref_index);
};
