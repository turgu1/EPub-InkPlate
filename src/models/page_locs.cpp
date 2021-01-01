#define __PAGE_LOCS__ 1
#include "models/page_locs.hpp"

#include "viewers/book_viewer.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "logging.hpp"

static constexpr char const * TAG = "PageLocs";

static xQueueHandle mgr_queue;
static xQueueHandle state_queue;
static xQueueHandle retrieve_queue;

enum class MgrReq : int8_t { DOCUMENT_COMPLETED, ASAP_READY };

struct MgrQueueData {
  MgrReq req;
  int16_t itemref_index;
};

enum class StateReq  : int8_t { STOP, START_DOCUMENT, GET_ASAP, ITEM_READY, ASAP_READY };

struct StateQueueData {
  StateReq req;
  int16_t itemref_index;
  int16_t itemref_count;
};

enum class RetrieveReq  : int8_t { RETRIEVE_ITEM, GET_ASAP };

struct RetrieveQueueData {
  RetrieveReq req;
  int16_t itemref_index;
};

static int16_t   itemref_count       = -1;      // Number of items in the document
static int16_t   waiting_for_itemref = -1;      // Current item being processed by retrieval task
static int16_t   next_itemref_to_get = -1;      // Non prioritize item to get next
static int16_t   asap_itemref        = -1;      // Prioritize item to get next
static uint8_t * bitset              = nullptr; // Set of all items processed so far
static uint8_t   bitset_size         =  0;      // bitset byte length
static bool      forget_retrieval    = false;   // Forget current item begin processed by retrieval task

static StateQueueData       state_queue_data;
static RetrieveQueueData retrieve_queue_data;
static MgrQueueData           mgr_queue_data;

/**
 * @brief Request next item to be retrieved
 * 
 * This function is called to identify and send the
 * next request for retrieval of page locations. It also
 * identify when the whole process is completed, as all items from
 * the document have been done. It will then send this information
 * to the appliction through the Mgr queue.
 * 
 * When this function is called, the retrieval task is waiting for
 * the next task to do.
 * 
 * @param itemref The last itemref index that was processed
 */
static void
request_next_item(int16_t itemref, bool already_sent_to_mgr = false)
{
  if (asap_itemref != -1) {
    if (itemref == asap_itemref) {
      asap_itemref = -1;
      if (!already_sent_to_mgr) {
        mgr_queue_data.itemref_index = itemref;
        mgr_queue_data.req = MgrReq::ASAP_READY;
        xQueueSend(mgr_queue, &mgr_queue_data, 0);
      }
    }
    else {
      waiting_for_itemref = asap_itemref;
      asap_itemref        = -1;
      retrieve_queue_data.req = RetrieveReq::GET_ASAP;
      retrieve_queue_data.itemref_index = waiting_for_itemref;
      xQueueSend(retrieve_queue, &retrieve_queue_data, 0);
      return;
    }
  }
  if (next_itemref_to_get != -1) {
    waiting_for_itemref = next_itemref_to_get;
    next_itemref_to_get = -1;
    retrieve_queue_data.req = RetrieveReq::RETRIEVE_ITEM;
    retrieve_queue_data.itemref_index = waiting_for_itemref;
    xQueueSend(retrieve_queue, &retrieve_queue_data, 0);
  }
  else {
    int16_t newref;
    if (itemref != -1) {
      newref = (itemref + 1) % itemref_count;
    }
    else {
      itemref = 0;
      newref  = 0;
    }
    while ((bitset[newref >> 3] & (1 << (newref & 7))) != 0) {
      newref = (newref + 1) % itemref_count;
      if (newref == itemref) break;
    }
    if (newref != itemref) {
      waiting_for_itemref = newref;
      retrieve_queue_data.req = RetrieveReq::RETRIEVE_ITEM;
      retrieve_queue_data.itemref_index = waiting_for_itemref;
      xQueueSend(retrieve_queue, &retrieve_queue_data, 0);
    }
    else {
      mgr_queue_data.req = MgrReq::DOCUMENT_COMPLETED;
      xQueueSend(mgr_queue, &mgr_queue_data, 0);
    } 
  }
}

static void
state_task(void * param)
{
  for (;;) {
    xQueueReceive(state_queue, &state_queue_data, portMAX_DELAY);
    switch (state_queue_data.req) {
      case StateReq::STOP:
        itemref_count = -1;
        forget_retrieval = true;
        if (bitset != nullptr) delete [] bitset;
        break;

      case StateReq::START_DOCUMENT:
        if (bitset) delete [] bitset;
        itemref_count = state_queue_data.itemref_count;
        bitset_size   = (itemref_count + 7) >> 3;
        bitset        = new uint8_t[bitset_size];
        if (bitset) {
          memset(bitset, 0, bitset_size);
          if (waiting_for_itemref == -1) {
            forget_retrieval = false;
            retrieve_queue_data.req = RetrieveReq::RETRIEVE_ITEM;
            waiting_for_itemref = retrieve_queue_data.itemref_index = state_queue_data.itemref_index;
            xQueueSend(retrieve_queue, &retrieve_queue_data, 0);
          }
          else {
            forget_retrieval = true;
            next_itemref_to_get = state_queue_data.itemref_index;
          }
        }
        else {
          itemref_count = -1;
        }
        break;

      case StateReq::GET_ASAP:
        // Mgr request a specific item. If document retrieval not started, 
        // return a negative value.
        // If already done, let it know it a.s.a.p. If currently being processed,
        // keep a mark when it will be back. If not, queue the request.
        if (itemref_count == -1) {
          mgr_queue_data.req           = MgrReq::ASAP_READY;
          mgr_queue_data.itemref_index = -(state_queue_data.itemref_index + 1);
          xQueueSend(mgr_queue, &mgr_queue_data, 0);
        }
        else {
          int16_t itemref = state_queue_data.itemref_index;
          if ((bitset[itemref >> 3] & ( 1 << (itemref & 7))) != 0) {
            mgr_queue_data.req           = MgrReq::ASAP_READY;
            mgr_queue_data.itemref_index = itemref;
            xQueueSend(mgr_queue, &mgr_queue_data, 0);
          }
          else if (waiting_for_itemref != -1) {
            asap_itemref = itemref;
          }
          else {
            asap_itemref                      = -1;
            waiting_for_itemref               = itemref;
            retrieve_queue_data.req           = RetrieveReq::GET_ASAP;
            retrieve_queue_data.itemref_index = itemref;
            xQueueSend(retrieve_queue, &retrieve_queue_data, 0);             
          }
        }
        break;

      // This is sent by the retrieval task, indicating that an item has been
      // processed.
      case StateReq::ITEM_READY:
        waiting_for_itemref = -1;
        if (itemref_count != -1) {
          int16_t itemref = -1;
          if (forget_retrieval) {
            forget_retrieval    = false;
          }
          else {
            itemref = state_queue_data.itemref_index;
            if (itemref < 0) {
              itemref = -(itemref + 1);
              LOG_I("Unable to retrieve page locations for item %d", itemref);
            }
            bitset[itemref >> 3] |= (1 << (itemref & 7));
          }
          request_next_item(itemref);
        }
        break;

      // This is sent by the retrieval task, indicating that an ASAP item has been
      // processed.
      case StateReq::ASAP_READY:
        waiting_for_itemref = -1;
        if (itemref_count != -1) {
          int16_t itemref              = state_queue_data.itemref_index;
          mgr_queue_data.itemref_index = itemref;
          mgr_queue_data.req           = MgrReq::ASAP_READY;
          xQueueSend(mgr_queue, &mgr_queue_data, 0);
          if (itemref < 0) {
            itemref = -(itemref + 1);
            LOG_I("Unable to retrieve page locations for item %d", itemref);
          }
          bitset[itemref >> 3] |= (1 << (itemref & 7));
          request_next_item(itemref, true);
        }
        break;
    }
  }
}

static void
retriever_task(void * param)
{
  RetrieveQueueData retrieve_queue_data;
  StateQueueData    state_queue_data;

  for (;;) {
    xQueueReceive(retrieve_queue, &retrieve_queue_data, portMAX_DELAY);

    LOG_I("Retrieving itemref %d", retrieve_queue_data.itemref_index);

    if (!book_viewer.build_page_locs(retrieve_queue_data.itemref_index)) {
      // Unable to retrieve pages location for the requested index. Send back
      // a negative value to indicate the issue to the state task
      state_queue_data.itemref_index = -(retrieve_queue_data.itemref_index + 1);
    }
    else {
      state_queue_data.itemref_index = retrieve_queue_data.itemref_index;
    }

    state_queue_data.req = 
      retrieve_queue_data.req == RetrieveReq::GET_ASAP ? 
        StateReq::ASAP_READY : StateReq::ITEM_READY;

    xQueueSend(state_queue, &state_queue_data, 0);
  }
}

void
PageLocs::setup()
{
  mgr_queue      = xQueueCreate(5, sizeof(MgrQueueData));
  state_queue    = xQueueCreate(5, sizeof(StateQueueData));
  retrieve_queue = xQueueCreate(5, sizeof(RetrieveQueueData));

  TaskHandle_t xHandle = NULL;

  xTaskCreate(state_task, "stateTask", 10000, (void *) 1, tskIDLE_PRIORITY, &xHandle);
  configASSERT(xHandle);

  #if EPUB_LINUX_BUILD
    xTaskCreate(retriever_task, "retrieverTask", 40000, (void *) 1, tskIDLE_PRIORITY, &xHandle);
    configASSERT(xHandle);
  #else
    xTaskCreatePinnedToCore(retriever_task, "retrieverTask", 40000, (void *) 1, tskIDLE_PRIORITY, &xHandle, 1);
    configASSERT(xHandle);
  #endif
} 

bool 
PageLocs::retrieve_asap(int16_t itemref_index) {
  book_viewer.build_page_locs();
  computation_completed();
  show();
  return true;
}

const PageLocs::PageId * 
PageLocs::get_next_page_id(const PageId & page_id, int16_t count) 
{
  PagesMap::iterator it = check_and_find(page_id);
  if (it == pages_map.end()) return &check_and_find(PageId(0,0))->first;
  PageId id = page_id;

  for (int16_t cptr = count; cptr > 0; cptr--) {
    PagesMap::iterator prev = it;
    id.offset += it->second.size;
    it = pages_map.find(id);
    if (it == pages_map.end()) {
      id.itemref_index += 1; id.offset = 0;
      it = check_and_find(id);
      if (it == pages_map.end()) {
        if (count > 1) return &prev->first;
        else return &check_and_find(PageId(0,0))->first;
      }
    }
  }
  return &it->first;
}

const PageLocs::PageId * 
PageLocs::get_prev_page_id(const PageId & page_id, int count) 
{
  PagesMap::iterator it = check_and_find(page_id);
  if (it == pages_map.end()) return &check_and_find(PageId(0,0))->first;
  PageId id = page_id;
    
  for (int16_t cptr = count; cptr > 0; cptr--) {
    if (id.offset == 0) {
      if (id.itemref_index == 0) {
        if (count == 1) id.itemref_index = item_count - 1;
        else return &it->first;
      }
      else id.itemref_index--;
      if (items_set.find(id.itemref_index) == items_set.end()) retrieve_asap(id.itemref_index);
    }
    
    if (it == pages_map.begin()) it = pages_map.end();
    it--;
    id = it->first;
  }
  return &it->first;
}

const PageLocs::PageId * 
PageLocs::get_page_id(const PageId & page_id) 
{
  PagesMap::iterator it   = check_and_find(PageId(page_id.itemref_index, 0));
  PagesMap::iterator res  = pages_map.end();
  while ((it != pages_map.end()) && (it->first.itemref_index == page_id.itemref_index)) {
    if ((it->first.offset <= page_id.offset) && ((it->first.offset + it->second.size) > page_id.offset)) { res = it; break; }
    it++;
  }
  return (res == pages_map.end()) ? nullptr : &res->first ;
}

