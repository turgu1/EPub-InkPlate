#include "page_locs_retriever.hpp"

#include "models/config.hpp"
#include "models/fonts.hpp"
#include "models/page_locs_interpreter.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/screen_bottom.hpp"

#if EPUB_INKPLATE_BUILD
  #include "esp.hpp"

  QueueHandle_t PageLocsRetriever::retriever_queue{nullptr};
#else
  #include <mqueue.h>
  mqd_t PageLocsRetriever::retriever_queue{-1};
#endif

auto PageLocsRetriever::setup(const std::string &epub_filename) -> bool {

  epub = EPub::Make();
  if (epub == nullptr) {
    LOG_E("Unable to open the book file");
    return false;
  }

  // The following will restricts cleaning fonts and unzip classes from EPub destructor.
  // This is required to not interfere with main epub instance.
  epub->set_page_locs_instance(true);

  if (!epub->open(epub_filename)) {
    LOG_E("Unable to open the book");
    return false;
  }

  current_format_params = *epub->get_book_format_params();

  #if EPUB_LINUX_BUILD
    mq_unlink("/retriever");

    retriever_queue = mq_open("/retriever", O_RDWR | O_CREAT, S_IRWXU, &retriever_attr);
    if (retriever_queue == -1) {
      LOG_E("Unable to open retriever_queue: %d", errno);
      return false;
    }

    retriever_thread = std::thread(&PageLocsRetriever::task, this);
  #else
    if (retriever_queue == nullptr) {
      retriever_queue = xQueueCreate(5, sizeof(PageLocsRetriever::QueueData));
      if (retriever_queue == nullptr) {
        LOG_E("Unable to create retriever_queue");
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
    retriever_thread = std::thread(&PageLocsRetriever::task, this);
  #endif

  return true;
}

void PageLocsRetriever::wait_for_exit() {
  retriever_thread.join();
  retriever_thread.~thread();
}

void PageLocsRetriever::task() {
  PageLocsState::QueueData state_queue_data;

  for (;;) {
    LOG_D("==> Waiting for request... <==");
    QueueData queue_data;
    if (receive(queue_data) == -1) {
      LOG_E("Receive error: %d: %s", errno, strerror(errno));
    } else {
      if (queue_data.req == Req::ABORT) return;
      if (queue_data.req == Req::SHOW_HEAP) {
        #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
          ESP::show_heaps_info();
        #endif
        continue;
      }

      LOG_D("-> %s <-", (queue_data.req == Req::GET_ASAP) ? "GET_ASAP" : "RETRIEVE_ITEM");

      LOG_D("Retrieving itemref --> %d <--", queue_data.itemref_index);

      int16_t itemref_index;
      if (!build_page_locs(queue_data.itemref_index)) {
        // Unable to retrieve pages location for the requested index. Send back
        // a negative value to indicate the issue to the state task
        itemref_index = -(queue_data.itemref_index + 1);
      } else {
        itemref_index = queue_data.itemref_index;
      }

      // std::this_thread::sleep_for(std::chrono::seconds(5));
      PageLocsState::QueueData state_queue_data = {.req = (queue_data.req == Req::GET_ASAP)
                                                              ? PageLocsState::Req::ASAP_READY
                                                              : PageLocsState::Req::ITEM_READY,
                                                   .itemref_index = itemref_index,
                                                   .itemref_count = 0};

      PageLocsState::send(state_queue_data);
      LOG_D("Sent %s to State",
            (state_queue_data.req == PageLocsState::Req::ASAP_READY) ? "ASAP_READY" : "ITEM_READY");
    }
  }
}

bool PageLocsRetriever::build_page_locs(int16_t itemref_index) {
  // std::scoped_lock guard(book_viewer.get_mutex());

  Font *font  = fonts.get(ScreenBottom::FONT);
  page_bottom = font->get_line_height(ScreenBottom::FONT_SIZE) +
                (font->get_line_height(ScreenBottom::FONT_SIZE) >> 1);

  // page_out->set_compute_mode(Page::ComputeMode::LOCATION);

  // show_pictures = current_format_params.show_pictures == 1;

  bool done = false;

  EPub::ItemInfo item_info;

  if (epub->get_item_at_index(itemref_index, item_info)) {

    int16_t idx;

    if ((idx = fonts.get_index("Fontbase", Fonts::FaceStyle::NORMAL)) == -1) {
      idx = 3;
    }

    int8_t font_size = current_format_params.font_size;

    int8_t show_title = 0;
    config.get(Config::Ident::SHOW_TITLE, &show_title);

    uint16_t page_top = 0;

    if (show_title != 0) {
      Font *title_font = fonts.get(BookViewer::TITLE_FONT);
      page_top         = title_font->get_chars_height(BookViewer::TITLE_FONT_SIZE) + 10;
    }

    Page::Format fmt = {
        .line_height_factor = 0.95,
        .font_index         = idx,
        .font_size          = font_size,
        .screen_top         = page_top,
        .screen_bottom      = page_bottom,
    };

    auto dom      = DOM::Make();
    auto page_out = Page::Make();

    auto interp =
        PageLocsInterpreter::Make(epub, page_out, dom, Page::ComputeMode::LOCATION, item_info);

    #if DEBUGGING_AID
      interp->set_pages_to_show_state(PAGE_FROM, PAGE_TO);
      interp->check_page_to_show(pages_map.size());
    #endif

    interp->set_limits(0, 9999999, current_format_params.show_pictures == 1);

    while (!done) {

      // current_offset       = 0;
      // start_of_page_offset = 0;
      xml_node node;

      if ((node = item_info.xml_doc.child("html").child("body"))) {

        page_out->start(fmt);

        // #if EPUB_INKPLATE_BUILD
        //   esp_task_wdt_reset();
        // #endif

        Page::Format *new_fmt = interp->duplicate_fmt(fmt);
        if (!interp->build_pages_recurse(node, *new_fmt, dom->body, 1)) {
          interp->release_fmt(new_fmt);
          LOG_D("html parsing issue or aborted by Mgr");
          break;
        }
        interp->release_fmt(new_fmt);

        if (page_out->some_data_waiting()) page_out->end_paragraph(fmt);
      } else {
        LOG_D("No <body>");
        break;
      }

      interp->doc_end(fmt);

      done = true;
    }

    // dom->show();
  }

  // page_out->set_compute_mode(Page::ComputeMode::DISPLAY);

  item_info.css.reset();

  return done;
}
