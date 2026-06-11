#include "page_locs_retriever.hpp"

#include "config.hpp"
#include "fonts.hpp"
#include "models/page_locs.hpp"
#include "models/page_locs_interpreter.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/screen_bottom.hpp"

namespace {
  constexpr const char *TAG         = "PageLocsRetriever";
  constexpr int         kControlSendRetries = 3;

  auto sendToControlChecked(const PageLocsControl::QueueData &msg, const char *ctx,
                            bool critical = false) -> bool {
    const int attempts = critical ? kControlSendRetries : 1;
    for (int i = 0; i < attempts; ++i) {
      if (PageLocsControl::sendFromRetriever(msg) >= 0) { return true; }
      pageLocs.telemetry.queueSendFailures.fetch_add(1);
      LOG_W("{}: PageLocsControl::sendFromRetriever failed (attempt {}/{})", ctx, i + 1, attempts);
    }
    if (critical) {
      LOG_E("{}: critical control send failed after retries", ctx);
    }
    return false;
  }
} // namespace

#if EPUB_INKPLATE_BUILD
  #include "esp.hpp"
  #include "freertos/task.h"

  QueueHandle_t PageLocsRetriever::retrieverQueue{ nullptr };
#else
  #include <mqueue.h>

  mqd_t PageLocsRetriever::retrieverQueue{ -1 };
#endif

/**
 * Initialize retriever dependencies, queue, and worker thread.
 */
auto PageLocsRetriever::setup(const HimemString &epubFilename) -> bool {

  epub = EPub::Make();
  if (epub == nullptr) {
    LOG_E("Unable to open the book file");
    return false;
  }

  // The following will restricts cleaning fonts and unzip classes from EPub destructor
  // and prepare the Table of content to be repopulated.
  // This is required to not interfere with main epub instance.
  epub->setPageLocsInstance(true);
  epub->getFonts().setGlyphLoadMode(Font::GlyphLoadMode::METRICS_ONLY);

  if (!epub->open(epubFilename)) {
    LOG_E("Unable to open the book");
    return false;
  }

  #if EPUB_LINUX_BUILD
    mq_unlink("/retriever");

    retrieverQueue = mq_open("/retriever", O_RDWR | O_CREAT, S_IRWXU, &retrieverAttr);
    if (retrieverQueue == -1) {
      LOG_E("Unable to open retrieverQueue: {}", errno);
      return false;
    }

    retrieverThread = std::thread(&PageLocsRetriever::task, this);
  #else
    if (retrieverQueue == nullptr) {
      retrieverQueue = xQueueCreate(5, sizeof(PageLocsRetriever::QueueData));
      if (retrieverQueue == nullptr) {
        LOG_E("Unable to create retrieverQueue");
        return false;
      }
    } else {
      xQueueReset(retrieverQueue);
    }

    if (pdPASS != xTaskCreatePinnedToCore(
          [](void *param) {
    auto *self = static_cast<PageLocsRetriever *>(param);
    self->task();
    vTaskDelete(nullptr);
  }, "retrieverTask", 25 * 1024, this, (configMAX_PRIORITIES - 2) | portPRIVILEGE_BIT, nullptr,
          1)) {
      LOG_E("Unable to create retriever task");
      return false;
    }

    retrieverTaskHandle = xTaskGetHandle("retrieverTask");

  #endif

  return true;
}

/**
 * Join retriever worker thread during teardown.
 */
auto PageLocsRetriever::waitForExit() -> void {
  #if EPUB_LINUX_BUILD
    if (retrieverThread.joinable()) { retrieverThread.join(); }
  #else
    // Signal the retriever task to stop and wait for it to exit before returning.
    requestStop();
    while (eTaskGetState(retrieverTaskHandle) != eDeleted) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  #endif
}

/**
 * Static callback adapter used by the interpreter at page boundaries.
 */
auto PageLocsRetriever::pollPendingQueueAtPageBoundary(void *context) -> bool {
  return static_cast<PageLocsRetriever *>(context)->handlePendingQueueAtPageBoundary();
}

/**
 * Drain and process pending retriever messages at safe interruption points.
 *
 * Returns true when current item should be aborted (ASAP or STOP).
 */
auto PageLocsRetriever::handlePendingQueueAtPageBoundary() -> bool {
  bool shouldAbort = false;

  for (;;) {
    QueueData queueData;

    #if EPUB_LINUX_BUILD
      mq_attr attr{};
      if (mq_getattr(retrieverQueue, &attr) == -1) {
        LOG_E("Unable to query retrieverQueue: {}", errno);
        return shouldAbort;
      }
      if (attr.mq_curmsgs == 0) { break; }
      if (receive(queueData) == -1) {
        LOG_E("Unable to receive retrieverQueue message: {}", errno);
        return shouldAbort;
      }
    #else
      if (receive(queueData, 0) != pdTRUE) { break; }
    #endif

    switch (queueData.req) {
    case Req::GET_ASAP:
      if (queueData.itemrefIndex == currentItemrefIndex) {
        SHOW_IT("Page boundary: ignoring GET_ASAP for current itemref {}", queueData.itemrefIndex);
      } else {
        SHOW_IT("{:40c} ----> PB GET_ASAP ({})", ' ', queueData.itemrefIndex);
        abortCurrentItem.store(true, std::memory_order_relaxed);
        shouldAbort = true;
      }
      break;

    case Req::STOP:
      SHOW_IT("{:40c} ----> PB STOP", ' ');
      stopRequested.store(true, std::memory_order_relaxed);
      abortCurrentItem.store(true, std::memory_order_relaxed);
      return true;

    case Req::SHOW_HEAP:
      SHOW_IT("{:40c} ----> PB SHOW_HEAP", ' ');
      #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
        ESP::show_heaps_info();
      #endif
      break;

    case Req::NONE:
      SHOW_IT("{:40c} ----> PB NONE", ' ');
      break;

    case Req::RETRIEVE_ITEM:
    case Req::COMPLETED:
      LOG_W("Ignoring unexpected retriever queue message {} at page boundary",
            static_cast<int>(queueData.req));
      break;
    }
  }

  return shouldAbort;
}

/**
 * Main retriever loop.
 *
 * Waits for work commands, performs item retrieval, and exits on STOP.
 */
auto PageLocsRetriever::task() -> void {
  for (;;) {
    LOG_D("==> Waiting for request... <==");
    QueueData queueData;
    if (receive(queueData) == -1) {
      SHOW_IT("{:40c} ----> Error: {}: {}", ' ', errno, strerror(errno));
    } else {
      switch (queueData.req) {
      case Req::NONE:
        SHOW_IT("{:40c} ----> NONE", ' ');
        break;

      case Req::STOP:
        SHOW_IT("{:40c} ----> STOP", ' ');
        // SHOW_IT("Received STOP request. Exiting retriever task...");
        return;

      case Req::GET_ASAP:
      case Req::RETRIEVE_ITEM: {
        #if EPUB_INKPLATE_BUILD
          SHOW_IT("{:40c} ---->  {} ({}, {})", ' ',
                  (queueData.req == Req::GET_ASAP) ? "GET_ASAP" : "RETRIEVE_ITEM",
                  queueData.itemrefIndex, (int32_t)uxTaskGetStackHighWaterMark(nullptr));
        #else
          SHOW_IT("{:40c} ----> {} ({})", ' ',
                  (queueData.req == Req::GET_ASAP) ? "GET_ASAP" : "RETRIEVE_ITEM",
                  queueData.itemrefIndex);
        #endif
        retrieve_item(queueData.req, queueData.itemrefIndex, queueData.correlationId);
        if (stopRequested.load(std::memory_order_relaxed)) {
          SHOW_IT("Stop requested during retrieval. Exiting retriever task...");
          return;
        }
      } break;

      case Req::SHOW_HEAP:
        SHOW_IT("{:40c} ----> SHOW_HEAP", ' ');
        #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
          ESP::show_heaps_info();
        #endif
        break;

      case Req::COMPLETED:
        SHOW_IT("{:40c} ----> COMPLETED", ' ');
        epub->toc->save(epub->getCurrentFilename());
        break;
      }
    }
  }
}

/**
 * Retrieve one item and notify control with ITEM_READY, ASAP_READY, or
 * ITEM_INTERRUPTED depending on completion vs preemption outcome.
 */
auto PageLocsRetriever::retrieve_item(Req req, int16_t itemrefIndex, uint32_t correlationId)
-> void {

  // SHOW_IT("Retrieving itemref --> {} <--", itemrefIndex);

  stopRequested.store(false, std::memory_order_relaxed);

  // Clear any stale abort signal before starting this item.
  abortCurrentItem.store(false, std::memory_order_relaxed);

  bool                       done = buildPageLocs(itemrefIndex);

  bool                       aborted  = abortCurrentItem.load(std::memory_order_relaxed);
  bool                       stopping = stopRequested.load(std::memory_order_relaxed);

  PageLocsControl::QueueData controlQueueData;

  if (stopping && aborted && !done) {
    // SHOW_IT("Item {} interrupted by STOP; exiting retrieve_item without notifying ControlTask",
    // itemrefIndex);
    return;
  }

  if (aborted && !done) {
    // Interrupted mid-item by a higher-priority request.  Partial pages already
    // inserted into pagesMap are harmless — pageLocs.insert() is idempotent.
    // SHOW_IT("Item {} interrupted; sending ITEM_INTERRUPTED to ControlTask", itemrefIndex);
    controlQueueData = { .req           = PageLocsControl::Req::ITEM_INTERRUPTED,
                         .itemrefIndex  = itemrefIndex,
                         .itemrefCount  = 0,
                         .correlationId = correlationId };
  } else {
    if (!done) {
      // Genuine failure (not an abort).
      itemrefIndex = static_cast<int16_t>(-(itemrefIndex + 1));
    }
    controlQueueData = { .req           = (req == Req::GET_ASAP) ? PageLocsControl::Req::ASAP_READY
                                                                : PageLocsControl::Req::ITEM_READY,
                         .itemrefIndex  = itemrefIndex,
                         .itemrefCount  = 0,
                         .correlationId = (req == Req::GET_ASAP) ? correlationId : 0 };
  }

  SHOW_IT("{:40c} <---- {} ({})", ' ',
          (controlQueueData.req == PageLocsControl::Req::ASAP_READY)   ? "ASAP_READY"
          : (controlQueueData.req == PageLocsControl::Req::ITEM_READY) ? "ITEM_READY"
                                                                       : "ITEM_INTERRUPTED",
          controlQueueData.itemrefIndex);
  sendToControlChecked(controlQueueData, "retrieve_item/notify_control", true);
}

/**
 * Compute page boundaries for one spine item and insert boundaries into
 * PageLocs map through the interpreter path.
 */
auto PageLocsRetriever::buildPageLocs(int16_t itemrefIndex) -> bool {
  Fonts &  fonts = epub->getFonts();

  FontPtr &font = fonts.getFont(ScreenBottom::FONT);
  pageBottom    = font->getLineHeight(ScreenBottom::FONT_SIZE) + 10;

  bool           resultOk = false;

  EPub::ItemInfo itemInfo;

  if (epub->getItemAtIndex(itemrefIndex, itemInfo)) {

    currentItemrefIndex = itemrefIndex;

    int16_t idx;

    if ((idx = fonts.getFontIndex("Fontbase", FaceStyle::NORMAL)) == -1) {
      idx = 3;
    }
    int8_t showTitle = 0;
    config.get(Config::Ident::SHOW_TITLE, &showTitle);

    uint16_t pageTop = 0;

    if (showTitle != 0) {
      FontPtr &titleFont = fonts.getFont(BookViewer::TITLE_FONT);
      pageTop            = titleFont->getCharsHeight(BookViewer::TITLE_FONT_SIZE) + 10;
    }

    Page::Format fmt = {
      .lineHeightFactor = epub->getLineHeightFactor(),
      .fontIndex        = idx,
      .fontSize         = epub->getBookFormatParams()->fontSize,
      .screenTop        = pageTop,
      .screenBottom     = pageBottom,
    };

    auto         dom     = DOM::Make(epub->getDomPools());
    auto         pageOut = Page::Make(fonts);

    auto         interp = PageLocsInterpreter::Make(
      epub, pageOut, dom, Page::ComputeMode::LOCATION, itemInfo, abortCurrentItem,
      &PageLocsRetriever::pollPendingQueueAtPageBoundary, this);

    #if DEBUGGING_AID
      interp->setPagesToShowState(PAGE_FROM, PAGE_TO);
      interp->checkPageToShow(pages_map.size());
    #endif

    // In PageLocs mode we only need stable text flow boundaries.ure.
    interp->setLimits(0, 9999999, epub->getBookFormatParams()->showPictures != 0);

    xml_node node;

    if ((node = itemInfo.xmlDoc.child("html").child("body"))) {

      pageOut->start(fmt, epub->getBookFormatParams()->columnCount);

      // newFmt is required to be able to modify the format in the recursive calls without
      // interfering with the original one used for page start
      Page::Format *newFmt = interp->duplicateFmt(fmt);

      if (!interp->buildPagesRecurse(node, *newFmt, dom->body, 1)) {
        interp->releaseFmt(newFmt);
        LOG_D("html parsing issue or aborted");
      } else {
        interp->releaseFmt(newFmt);

        if (pageOut->someDataWaiting()) { pageOut->endParagraph(fmt); }

        // Remaining processing at the end of the document to be sure to
        // include the last page if not already done
        interp->docEnd(fmt);

        resultOk = true;
      }
    } else {
      LOG_D("No <body>");
    }
  }

  return resultOk;
}
