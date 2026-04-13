// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PAGE_LOCS__ 1
#include "models/page_locs.hpp"

#include "controllers/event_mgr.hpp"
#include "models/config.hpp"
#include "models/page_locs_state.hpp"

#include "viewers/book_viewer.hpp"
#include "viewers/page.hpp"

#include <fstream>
#include <ios>
#include <iostream>

#if EPUB_LINUX_BUILD
  #include <chrono>
#else
  #include <esp_pthread.h>
#endif

#if EPUB_INKPLATE_BUILD
  QueueHandle_t PageLocs::mgr_queue{nullptr};
#else
  #include <mqueue.h>
  mqd_t PageLocs::mgr_queue{-1};
#endif

void PageLocs::setup_pages_computation(EPubPtr &epub) {

  abort_threads();

  state_task = PageLocsState::Make();
  state_task->setup(epub->get_current_filename());

  #if EPUB_LINUX_BUILD
    mq_unlink("/mgr");

    mgr_queue = mq_open("/mgr", O_RDWR | O_CREAT, S_IRWXU, &mgr_attr);
    if (mgr_queue == -1) {
      LOG_E("Unable to open mgr_queue: %d", errno);
      return;
    }

  #else
    esp_pthread_init();

    if (mgr_queue == nullptr) mgr_queue = xQueueCreate(5, sizeof(QueueData));

  #endif
}

void PageLocs::abort_threads() {
  if (state_task) {
    LOG_D("abort_threads: Sending ABORT to State");
    PageLocsState::send({.req = PageLocsState::Req::ABORT});

    state_task->wait_for_exit();
    state_task.reset();
  }
}

volatile bool relax = false;

bool PageLocs::retrieve_asap(int16_t itemref_index) {
  LOG_D("retrieve_asap: Sending GET_ASAP");
  PageLocsState::send(
      {.req = PageLocsState::Req::GET_ASAP, .itemref_index = itemref_index, .itemref_count = 0});

  relax = true;
  QueueData queue_data;
  LOG_D("==> Waiting for answer... <==");
  receive(queue_data);
  LOG_D("-> %s <-", queue_data.req == Req::ASAP_READY ? "ASAP_READY" : "ERROR!!!");
  relax = false;

  return true;
}

void PageLocs::stop_document() {

  if (!completed) {
    if (state_task) {
      LOG_D("Sending STOP");
      PageLocsState::send({.req = PageLocsState::Req::STOP});

      QueueData queue_data;
      LOG_D("==> Waiting for STOPPED... <==");
      receive(queue_data);
      LOG_D("-> %s <-", (queue_data.req == Req::STOPPED) ? "STOPPED" : "ERROR!!!");

      state_task->wait_for_exit();
      state_task.reset();
    }
  }
}

int16_t PageLocs::get_page_count() {
  if (completed) return page_count;

  LOG_D("get_page_count: Sending PERCENT");
  PageLocsState::send({.req = PageLocsState::Req::PERCENT});

  relax = true;
  QueueData queue_data;
  LOG_D("==> Waiting for answer... <==");
  receive(queue_data);
  LOG_D("-> %s <-", queue_data.req == Req::PERCENT ? "PERCENT" : "ERROR!!!");
  relax = false;

  return queue_data.itemref_index;
}

void PageLocs::start_new_document(EPubPtr &epub, int16_t itemref_index) {
  if (state_task && !state_task->retriever_is_iddle()) stop_document();

  current_filename = epub->get_current_filename();
  // check_for_format_changes(epub, itemref_index, !load(current_filename));
}

bool PageLocs::insert(PageId &id, PageInfo &info) {
  if (!state_task || !state_task->forgetting_retrieval()) {
    while (true) {
      if (relax) {
        // The page_locs class is still in control of the mutex, but is waiting
        // for the completion of an GET_ASAP item. As such, it is safe to insert
        // a new page info in the list.
        LOG_D("Relaxed page info insert...");
        pages_map.insert(std::make_pair(id, info));
        items_set.insert(id.itemref_index);
        break;
      } else {
        if (mutex.try_lock_for(std::chrono::milliseconds(10))) {
          pages_map.insert(std::make_pair(id, info));
          items_set.insert(id.itemref_index);
          mutex.unlock();
          break;
        }
      }
    }
    return true;
  }
  return false;
}

PageLocs::PagesMap::iterator PageLocs::check_and_find(const PageId &page_id) {
  PagesMap::iterator it = pages_map.find(page_id);
  if (!completed && (it == pages_map.end())) {
    if (retrieve_asap(page_id.itemref_index)) it = pages_map.find(page_id);
  }
  return it;
}

const PageId *PageLocs::get_next_page_id(const PageId &page_id, int16_t count) {
  std::scoped_lock guard(mutex);

  PagesMap::iterator it = check_and_find(page_id);
  if (it == pages_map.end()) {
    it = check_and_find(PageId(0, 0));
  } else {
    PageId id = page_id;
    bool done = false;
    for (int16_t cptr = count; cptr > 0; cptr--) {
      PagesMap::iterator prev = it;
      do {
        id.offset += abs(it->second.size);
        it = pages_map.find(id);
        if (it == pages_map.end()) {
          // We have reached the end of the current item. Move to the next
          // item and try again
          id.itemref_index += 1;
          id.offset = 0;
          it        = check_and_find(id);
          if (it == pages_map.end()) {
            // We have reached the end of the list. If stepping one page at a time, go
            // to the first page
            it   = (count > 1) ? prev : check_and_find(PageId(0, 0));
            done = true;
          }
        }
      } while ((it->second.size < 0) && !done);
      if (done) break;
    }
  }
  return (it == pages_map.end()) ? nullptr : &it->first;
}

const PageId *PageLocs::get_prev_page_id(const PageId &page_id, int count) {
  std::scoped_lock guard(mutex);

  PagesMap::iterator it = check_and_find(page_id);
  if (it == pages_map.end()) {
    it = check_and_find(PageId(0, 0));
  } else {
    PageId id = it->first;

    bool done = false;
    for (int16_t cptr = count; cptr > 0; cptr--) {
      do {
        if (id.offset == 0) {
          if (id.itemref_index == 0) {
            if (count == 1)
              id.itemref_index = item_count - 1;
            else
              done = true;
          } else
            id.itemref_index--;

          if (items_set.find(id.itemref_index) == items_set.end()) {
            retrieve_asap(id.itemref_index);
          }
        }

        if (!done) {
          if (it == pages_map.begin()) it = pages_map.end();
          it--;
          id = it->first;
        }
      } while ((it->second.size < 0) && !done);
      if (done) break;
    }
  }
  return (it == pages_map.end()) ? nullptr : &it->first;
}

const PageId *PageLocs::get_page_id(const PageId &page_id) {
  std::scoped_lock guard(mutex);

  PagesMap::iterator it     = check_and_find(PageId(page_id.itemref_index, 0));
  PagesMap::iterator result = pages_map.end();
  while ((it != pages_map.end()) && (it->first.itemref_index == page_id.itemref_index)) {
    if ((it->first.offset == page_id.offset) ||
        ((it->first.offset < page_id.offset) &&
         ((it->first.offset + abs(it->second.size)) > page_id.offset))) {
      result = it;
      break;
    }
    it++;
  }
  return (result == pages_map.end()) ? nullptr : &result->first;
}

void PageLocs::computation_completed() {
  std::scoped_lock guard(mutex);

  if (!completed) {
    int16_t page_nbr = 0;
    for (auto &entry : pages_map) {
      if (entry.second.size >= 0) entry.second.page_number = page_nbr++;
    }

    page_count = page_nbr;

    save(current_filename);

    // show();

    completed = true;

    // epub->toc->save(epub);

    event_mgr.set_stay_on(false);

    // #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    //   ESP::show_heaps_info();
    //   RetrieveQueueData retrieve_queue_data = {
    //     .req = RetrieveReq::SHOW_HEAP,
    //     .itemref_index = 0
    //   };
    //   LOG_D("Sending SHOW_HEAP to Retriever");
    //   QUEUE_SEND(retrieve_queue, retrieve_queue_data, 0);
    // #endif
  }
}

#if DEBUGGING
  void PageLocs::show() {
    std::cout << "----- Page Locations -----" << std::endl;
    for (auto &entry : pages_map) {
      std::cout << " idx: " << entry.first.itemref_index << " off: " << entry.first.offset
                << " siz: " << entry.second.size << " pg: " << entry.second.page_number
                << std::endl;
    }
    std::cout << "----- End Page Locations -----" << std::endl;
  }
#endif

void PageLocs::check_for_format_changes(EPubPtr &epub, int16_t itemref_index, bool force) {
  if (force ||
      (memcmp(epub->get_book_format_params(), &current_format_params,
              sizeof(current_format_params)) != 0) ||
      !epub->toc->load(epub)) {

    LOG_D("==> Page locations recalc. <==");

    if (state_task && !state_task->retriever_is_iddle()) stop_document();

    clear();

    setup_pages_computation(epub);

    current_format_params = *epub->get_book_format_params();

    if (epub->toc->load_from_epub(epub) && !epub->toc->there_is_some_ids()) {
      // The table of content doesn't need to be synch with the
      // page location computation. I.e. there is no relation with HTML Ids
      // that would require information from the page location computation
      // to find where the table of content pages are located.
      epub->toc->save(epub);
    }

    LOG_D("start_new_document: Sending START_DOCUMENT");
    PageLocsState::send({.req           = PageLocsState::Req::START_DOCUMENT,
                         .itemref_index = itemref_index,
                         .itemref_count = epub->get_item_count()});

    event_mgr.set_stay_on(true);
  }
}

bool PageLocs::load(const std::string &epub_filename) {
  std::string filename = epub_filename.substr(0, epub_filename.find_last_of('.')) + ".locs";
  std::ifstream file(filename, std::ios::in | std::ios::binary);

  LOG_D("Loading pages location from file %s.", filename.c_str());

  int8_t version;
  int16_t pg_count;

  if (!file.is_open()) {
    LOG_I("Unable to open pages location file. Calculating locations...");
    return false;
  }

  bool ok = false;
  while (true) {
    if (file.read(reinterpret_cast<char *>(&version), 1).fail()) break;
    if (version != LOCS_FILE_VERSION) break;

    if (file.read(reinterpret_cast<char *>(&current_format_params), sizeof(current_format_params))
            .fail())
      break;
    if (file.read(reinterpret_cast<char *>(&pg_count), sizeof(pg_count)).fail()) break;

    pages_map.clear();

    int16_t page_nbr = 0;

    for (int16_t i = 0; i < pg_count; i++) {
      PageId page_id;
      PageInfo page_info;

      if (file.read(reinterpret_cast<char *>(&page_id.itemref_index), sizeof(page_id.itemref_index))
              .fail())
        break;
      if (file.read(reinterpret_cast<char *>(&page_id.offset), sizeof(page_id.offset)).fail())
        break;
      if (file.read(reinterpret_cast<char *>(&page_info.size), sizeof(page_info.size)).fail())
        break;
      page_info.page_number = (page_info.size >= 0) ? page_nbr++ : -1;

      page_locs.insert(page_id, page_info);
    }

    page_count = page_nbr;
    ok         = !file.fail();
    break;
  }

  file.close();

  LOG_D("Page locations load %s.", ok ? "Success" : "Error");

  completed = ok;

  return ok;
}

bool PageLocs::save(const std::string &epub_filename) {
  std::string filename = epub_filename.substr(0, epub_filename.find_last_of('.')) + ".locs";
  std::ofstream file(filename, std::ios::out | std::ios::binary);

  LOG_D("Saving pages location to file %s", filename.c_str());

  if (!file.is_open()) {
    LOG_E("Not able to open pages location file.");
    return false;
  }

  int16_t page_count = pages_map.size();

  while (true) {
    if (file.write(reinterpret_cast<const char *>(&LOCS_FILE_VERSION), 1).fail()) break;
    if (file.write(reinterpret_cast<const char *>(&current_format_params),
                   sizeof(current_format_params))
            .fail())
      break;
    if (file.write(reinterpret_cast<const char *>(&page_count), sizeof(page_count)).fail()) break;

    for (auto &page_map : pages_map) {
      if (file.write(reinterpret_cast<const char *>(&page_map.first.itemref_index),
                     sizeof(page_map.first.itemref_index))
              .fail())
        break;
      if (file.write(reinterpret_cast<const char *>(&page_map.first.offset),
                     sizeof(page_map.first.offset))
              .fail())
        break;
      if (file.write(reinterpret_cast<const char *>(&page_map.second.size),
                     sizeof(page_map.second.size))
              .fail())
        break;
    }

    break;
  }

  bool res = !file.fail();
  file.close();

  LOG_D("Page locations save %s.", res ? "Success" : "Error");

  return res;
}
