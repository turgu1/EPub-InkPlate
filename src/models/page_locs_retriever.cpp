#include "page_locs_retriever.hpp"

#include "models/config.hpp"
#include "models/fonts.hpp"
#include "models/page_locs_interpreter.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/screen_bottom.hpp"

#if EPUB_INKPLATE_BUILD
  #include "esp.hpp"

  QueueHandle_t PageLocsRetriever::retrieverQueue{nullptr};
#else
  #include <mqueue.h>

  mqd_t PageLocsRetriever::retrieverQueue{-1};
#endif

auto PageLocsRetriever::setup(const std::string &epubFilename) -> bool {

  epub = EPub::Make();
  if (epub == nullptr) {
    LOG_E("Unable to open the book file");
    return false;
  }

  // The following will restricts cleaning fonts and unzip classes from EPub destructor
  // and prepare the Table of content to be repopulated.
  // This is required to not interfere with main epub instance.
  epub->setPageLocsInstance(true);

  if (!epub->open(epubFilename)) {
    LOG_E("Unable to open the book");
    return false;
  }

  // In preparation to rebuild the pages location, we need to get the current
  // format parameters of the book.
  currentFormatParams = *epub->getBookFormatParams();

  #if EPUB_LINUX_BUILD
    mq_unlink("/retriever");

    retrieverQueue = mq_open("/retriever", O_RDWR | O_CREAT, S_IRWXU, &retrieverAttr);
    if (retrieverQueue == -1) {
      LOG_E("Unable to open retrieverQueue: %d", errno);
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
    }

    auto cfg        = esp_pthread_get_default_config();
    cfg.thread_name = "retrieverTask";
    cfg.pin_to_core = 1;
    cfg.stack_size  = 40 * 1024;
    cfg.prio        = configMAX_PRIORITIES - 2;
    cfg.inherit_cfg = true;

    esp_pthread_set_cfg(&cfg);
    retrieverThread = std::thread(&PageLocsRetriever::task, this);
  #endif

  return true;
}

auto PageLocsRetriever::waitForExit() -> void {
  retrieverThread.join();
  retrieverThread.~thread();
}

auto PageLocsRetriever::task() -> void {
  for (;;) {
    LOG_D("==> Waiting for request... <==");
    QueueData queueData;
    if (receive(queueData) == -1) {
      LOG_E("Receive error: %d: %s", errno, strerror(errno));
    } else {
      switch (queueData.req) {
      case Req::NONE:
        break;

      case Req::STOP:
        SHOW_IT("Received STOP request. Exiting retriever task...");
        return;

      case Req::RETRIEVE_ITEM:
      case Req::GET_ASAP: {
        SHOW_IT("-> %s <-", (queueData.req == Req::GET_ASAP) ? "GET_ASAP" : "RETRIEVE_ITEM");

        retrieve_item(queueData.req, queueData.itemrefIndex);

      } break;

      case Req::SHOW_HEAP:
        #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
          ESP::show_heaps_info();
        #endif
        break;

      case Req::COMPLETED:
        SHOW_IT("Received COMPLETED request. Saving the TOC...");
        epub->toc->save(epub->getCurrentFilename());
        break;
      }
    }
  }
}

auto PageLocsRetriever::retrieve_item(Req req, int16_t itemrefIndex) -> void {

  SHOW_IT("Retrieving itemref --> %d <--", itemrefIndex);

  if (!buildPageLocs(itemrefIndex)) {
    // Unable to retrieve pages location for the requested index. Send back
    // a negative value to indicate the issue to the control task
    itemrefIndex = -itemrefIndex;
  }

  // std::this_thread::sleep_for(std::chrono::seconds(5));
  PageLocsControl::QueueData controlQueueData = {.req = (req == Req::GET_ASAP)
                                                            ? PageLocsControl::Req::ASAP_READY
                                                            : PageLocsControl::Req::ITEM_READY,
                                                 .itemrefIndex = itemrefIndex,
                                                 .itemrefCount = 0};

  SHOW_IT("Sending %s to ControlTask",
          (controlQueueData.req == PageLocsControl::Req::ASAP_READY) ? "ASAP_READY" : "ITEM_READY");
  PageLocsControl::send(controlQueueData);
}

auto PageLocsRetriever::buildPageLocs(int16_t itemrefIndex) -> bool {
  // std::scoped_lock guard(book_viewer.getMutex());

  Font *font = fonts.get(ScreenBottom::FONT);
  pageBottom = font->getLineHeight(ScreenBottom::FONT_SIZE) +
               (font->getLineHeight(ScreenBottom::FONT_SIZE) >> 1);

  // pageOut->setComputeMode(Page::ComputeMode::LOCATION);

  // showPictures = current_format_params.showPictures == 1;

  bool resultOk = false;

  EPub::ItemInfo itemInfo;

  if (epub->getItemAtIndex(itemrefIndex, itemInfo)) {

    int16_t idx;

    if ((idx = fonts.getIndex("Fontbase", Fonts::FaceStyle::NORMAL)) == -1) {
      idx = 3;
    }

    int8_t fontSize = currentFormatParams.fontSize;

    int8_t showTitle = 0;
    config.get(Config::Ident::SHOW_TITLE, &showTitle);

    uint16_t pageTop = 0;

    if (showTitle != 0) {
      Font *titleFont = fonts.get(BookViewer::TITLE_FONT);
      pageTop         = titleFont->getCharsHeight(BookViewer::TITLE_FONT_SIZE) + 10;
    }

    Page::Format fmt = {
        .lineHeightFactor = 0.95,
        .fontIndex        = idx,
        .fontSize         = fontSize,
        .screenTop        = pageTop,
        .screenBottom     = pageBottom,
    };

    auto dom     = DOM::Make();
    auto pageOut = Page::Make();

    auto interp =
        PageLocsInterpreter::Make(epub, pageOut, dom, Page::ComputeMode::LOCATION, itemInfo);

    #if DEBUGGING_AID
      interp->setPagesToShowState(PAGE_FROM, PAGE_TO);
      interp->checkPageToShow(pages_map.size());
    #endif

    interp->setLimits(0, 9999999, currentFormatParams.showPictures == 1);

    // currentOffset       = 0;
    // start_of_page_offset = 0;
    xml_node node;

    if ((node = itemInfo.xmlDoc.child("html").child("body"))) {

      pageOut->start(fmt);

      // #if EPUB_INKPLATE_BUILD
      //   esp_task_wdt_reset();
      // #endif

      // newFmt is required to be able to modify the format in the recursive calls without
      // interfering with the original one used for page start
      Page::Format *newFmt = interp->duplicateFmt(fmt);

      if (!interp->buildPagesRecurse(node, *newFmt, dom->body, 1)) {
        interp->releaseFmt(newFmt);
        LOG_D("html parsing issue or aborted");
      } else {
        interp->releaseFmt(newFmt);

        if (pageOut->someDataWaiting()) pageOut->endParagraph(fmt);

        // Remaining processing at the end of the document to be sure to
        // include the last page if not already done
        interp->docEnd(fmt);

        resultOk = true;
      }
    } else {
      LOG_D("No <body>");
    }

    // dom->show();
  }

  // pageOut->setComputeMode(Page::ComputeMode::DISPLAY);

  // itemInfo.css.reset();

  return resultOk;
}
