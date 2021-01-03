#define __PAGE_LOCS__ 1
#include "models/page_locs.hpp"

#include "viewers/book_viewer.hpp"
#include "logging.hpp"

#include <chrono>

enum class MgrReq : int8_t { ASAP_READY };

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

#if EPUB_LINUX_BUILD
  static mqd_t mgr_queue;
  static mqd_t state_queue;
  static mqd_t retrieve_queue;

  static mq_attr mgr_attr      = { 0, 5, sizeof(     MgrQueueData), 0 };
  static mq_attr state_attr    = { 0, 5, sizeof(   StateQueueData), 0 };
  static mq_attr retrieve_attr = { 0, 5, sizeof(RetrieveQueueData), 0 };

  #define QUEUE_SEND(q, m, t)        mq_send(q, (const char *) &m, sizeof(m),       1)
  #define QUEUE_RECEIVE(q, m, t)  mq_receive(q,       (char *) &m, sizeof(m), nullptr)
#else
  static xQueueHandle mgr_queue;
  static xQueueHandle state_queue;
  static xQueueHandle retrieve_queue;

  #define QUEUE_SEND(q, m, t)        xQueueSend(q, &m, t)
  #define QUEUE_RECEIVE(q, m, t)  xQueueReceive(q, &m, t)
#endif

#if EPUB_LINUX_BUILD
  // pthread_mutex_t PageLocs::mutex;
#else
  SemaphoreHandle_t PageLocs::mutex = nullptr;
  StaticSemaphore_t PageLocs::mutex_buffer;
#endif

class StateTask
{
  private:
    static constexpr const char * TAG = "StateTask";

    bool retriever_iddle;

    int16_t   itemref_count;       // Number of items in the document
    int16_t   waiting_for_itemref; // Current item being processed by retrieval task
    int16_t   next_itemref_to_get; // Non prioritize item to get next
    int16_t   asap_itemref;        // Prioritize item to get next
    uint8_t * bitset;              // Set of all items processed so far
    uint8_t   bitset_size;         // bitset byte length
    bool      forget_retrieval;    // Forget current item begin processed by retrieval task

    StateQueueData       state_queue_data;
    RetrieveQueueData retrieve_queue_data;
    MgrQueueData           mgr_queue_data;

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
    void request_next_item(int16_t itemref,
                           bool already_sent_to_mgr = false)
    {
      if (asap_itemref != -1) {
        if (itemref == asap_itemref) {
          asap_itemref = -1;
          if (!already_sent_to_mgr) {
            mgr_queue_data.itemref_index = itemref;
            mgr_queue_data.req           = MgrReq::ASAP_READY;
            QUEUE_SEND(mgr_queue, mgr_queue_data, 0);
            LOG_D("Sent ASAP_READY to Mgr");
          }
        } else {
          waiting_for_itemref               = asap_itemref;
          asap_itemref                      = -1;
          retrieve_queue_data.req           = RetrieveReq::GET_ASAP;
          retrieve_queue_data.itemref_index = waiting_for_itemref;
          QUEUE_SEND(retrieve_queue, retrieve_queue_data, 0);
          LOG_D("Sent GET_ASAP to Retriever");
          return;
        }
      }
      if (next_itemref_to_get != -1) {
        waiting_for_itemref               = next_itemref_to_get;
        next_itemref_to_get               = -1;
        retrieve_queue_data.req           = RetrieveReq::RETRIEVE_ITEM;
        retrieve_queue_data.itemref_index = waiting_for_itemref;
        QUEUE_SEND(retrieve_queue, retrieve_queue_data, 0);
        LOG_D("Sent RETRIEVE_ITEM to Retriever");
      } else {
        int16_t newref;
        if (itemref != -1) {
          newref = (itemref + 1) % itemref_count;
        } else {
          itemref = 0;
          newref = 0;
        }
        while ((bitset[newref >> 3] & (1 << (newref & 7))) != 0) {
          newref = (newref + 1) % itemref_count;
          if (newref == itemref)
            break;
        }
        if (newref != itemref) {
          waiting_for_itemref               = newref;
          retrieve_queue_data.req           = RetrieveReq::RETRIEVE_ITEM;
          retrieve_queue_data.itemref_index = waiting_for_itemref;
          QUEUE_SEND(retrieve_queue, retrieve_queue_data, 0);
          LOG_D("Sent RETRIEVE_ITEM to Retriever");
        } else {
          page_locs.computation_completed();
          retriever_iddle = true;
        }
      }
    }

  public:
    StateTask() : 
          retriever_iddle(   true), 
            itemref_count(     -1),
      waiting_for_itemref(     -1),
      next_itemref_to_get(     -1),
             asap_itemref(     -1),
                   bitset(nullptr),
              bitset_size(      0),
         forget_retrieval(  false)  { }


    void operator()() {
      for (;;) {
        if (QUEUE_RECEIVE(state_queue, state_queue_data, portMAX_DELAY) == -1) {
          LOG_E("Receive error: %d: %s", errno, strerror(errno));
        }
        else switch (state_queue_data.req) {
          case StateReq::STOP:
            LOG_D("-> STOP <-");
            itemref_count    = -1;
            forget_retrieval = true;
            retriever_iddle  = true;
            if (bitset != nullptr)
              delete[] bitset;
            break;

          case StateReq::START_DOCUMENT:
            LOG_D("-> START_DOCUMENT <-");
            if (bitset) delete [] bitset;
            itemref_count = state_queue_data.itemref_count;
            bitset_size   = (itemref_count + 7) >> 3;
            bitset        = new uint8_t[bitset_size];
            if (bitset) {
              memset(bitset, 0, bitset_size);
              if (waiting_for_itemref == -1) {
                retrieve_queue_data.req           = RetrieveReq::RETRIEVE_ITEM;
                retrieve_queue_data.itemref_index = waiting_for_itemref =
                                                    state_queue_data.itemref_index;
                forget_retrieval                  = false;
                QUEUE_SEND(retrieve_queue, retrieve_queue_data, 0);
                LOG_D("Sent RETRIEVE_ITEM to retriever");
              }
              else {
                forget_retrieval    = true;
                next_itemref_to_get = state_queue_data.itemref_index;
              }
              retriever_iddle = false;
            }
            else {
              itemref_count = -1;
            }
            break;

          case StateReq::GET_ASAP:
            LOG_D("-> GET_ASAP <-");
            // Mgr request a specific item. If document retrieval not started, 
            // return a negative value.
            // If already done, let it know it a.s.a.p. If currently being processed,
            // keep a mark when it will be back. If not, queue the request.
            if (itemref_count == -1) {
              mgr_queue_data.req           = MgrReq::ASAP_READY;
              mgr_queue_data.itemref_index = -(state_queue_data.itemref_index + 1);
              QUEUE_SEND(mgr_queue, mgr_queue_data, 0);
              LOG_D("Sent ASAP_READY to Mgr");
            }
            else {
              int16_t itemref = state_queue_data.itemref_index;
              if ((bitset[itemref >> 3] & ( 1 << (itemref & 7))) != 0) {
                mgr_queue_data.req           = MgrReq::ASAP_READY;
                mgr_queue_data.itemref_index = itemref;
                QUEUE_SEND(mgr_queue, mgr_queue_data, 0);
                LOG_D("Sent ASAP_READY to Mgr");
              }
              else if (waiting_for_itemref != -1) {
                asap_itemref = itemref;
              }
              else {
                asap_itemref                      = -1;
                waiting_for_itemref               = itemref;
                retrieve_queue_data.req           = RetrieveReq::GET_ASAP;
                retrieve_queue_data.itemref_index = itemref;
                QUEUE_SEND(retrieve_queue, retrieve_queue_data, 0);       
                LOG_D("Sent GET_ASAP to Retriever"); 
              }
            }
            break;

          // This is sent by the retrieval task, indicating that an item has been
          // processed.
          case StateReq::ITEM_READY:
            LOG_D("-> ITEM_READY <-");
            waiting_for_itemref = -1;
            if (itemref_count != -1) {
              int16_t itemref = -1;
              if (forget_retrieval) {
                forget_retrieval = false;
              }
              else {
                itemref = state_queue_data.itemref_index;
                if (itemref < 0) {
                  itemref = -(itemref + 1);
                  LOG_E("Unable to retrieve page locations for item %d", itemref);
                }
                bitset[itemref >> 3] |= (1 << (itemref & 7));
              }
              request_next_item(itemref);
            }
            break;

          // This is sent by the retrieval task, indicating that an ASAP item has been
          // processed.
          case StateReq::ASAP_READY:
            LOG_D("-> ASAP_READY <-");
            waiting_for_itemref = -1;
            if (itemref_count != -1) {
              int16_t itemref              = state_queue_data.itemref_index;
              mgr_queue_data.itemref_index = itemref;
              mgr_queue_data.req           = MgrReq::ASAP_READY;
              QUEUE_SEND(mgr_queue, mgr_queue_data, 0);
              LOG_D("Sent ASAP_READY to Mgr");
              if (itemref < 0) {
                itemref = -(itemref + 1);
                LOG_E("Unable to retrieve page locations for item %d", itemref);
              }
              bitset[itemref >> 3] |= (1 << (itemref & 7));
              request_next_item(itemref, true);
            }
            break;
        }
      }
    }

    inline bool retriever_is_iddle() { return retriever_iddle; } 

} state_task;

class RetrieverTask
{
  private:
    static constexpr const char * TAG = "RetrieverTask";

  public:
    void operator ()() const {
      RetrieveQueueData retrieve_queue_data;
      StateQueueData state_queue_data;

      for (;;) {
        if (QUEUE_RECEIVE(retrieve_queue, retrieve_queue_data, portMAX_DELAY) == -1) {
          LOG_E("Receive error: %d: %s", errno, strerror(errno));
        }
        else {
          LOG_D("-> %s <-", (retrieve_queue_data.req == RetrieveReq::GET_ASAP) ? "GET_ASAP" : "RETRIEVE_ITEM");

          LOG_I("Retrieving itemref %d", retrieve_queue_data.itemref_index);

          if (!book_viewer.build_page_locs(retrieve_queue_data.itemref_index)) {
            // Unable to retrieve pages location for the requested index. Send back
            // a negative value to indicate the issue to the state task
            state_queue_data.itemref_index = -(retrieve_queue_data.itemref_index + 1);
          }
          else {
            state_queue_data.itemref_index = retrieve_queue_data.itemref_index;
          }

          std::this_thread::sleep_for(std::chrono::seconds(5));
          state_queue_data.req = 
            (retrieve_queue_data.req == RetrieveReq::GET_ASAP) ? 
              StateReq::ASAP_READY : StateReq::ITEM_READY;

          QUEUE_SEND(state_queue, state_queue_data, 0);
          LOG_D("Sent %s to State", (state_queue_data.req == StateReq::ASAP_READY) ? "ASAP_READY" : "ITEM_READY");
        }
      }
    }

} retriever_task;

void
PageLocs::setup()
{
  #if EPUB_LINUX_BUILD

    mq_unlink("/mgr");
    mq_unlink("/state");
    mq_unlink("/retrieve");

    mgr_queue      = mq_open("/mgr",      O_RDWR|O_CREAT, S_IRWXU, &mgr_attr);
    if (mgr_queue == -1) { LOG_E("Unable to open mgr_queue: %d", errno); return; }

    state_queue    = mq_open("/state",    O_RDWR|O_CREAT, S_IRWXU, &state_attr);
    if (state_queue == -1) { LOG_E("Unable to open state_queue: %d", errno); return; }

    retrieve_queue = mq_open("/retrieve", O_RDWR|O_CREAT, S_IRWXU, &retrieve_attr);
    if (retrieve_queue == -1) { LOG_E("Unable to open retrieve_queue: %d", errno); return; }

    retriever_thread = std::thread(retriever_task);
    state_thread     = std::thread(state_task);
  #else
    mgr_queue      = xQueueCreate(5, sizeof(MgrQueueData));
    state_queue    = xQueueCreate(5, sizeof(StateQueueData));
    retrieve_queue = xQueueCreate(5, sizeof(RetrieveQueueData));

    TaskHandle_t xHandle = NULL;

    xTaskCreate(state_task, "stateTask", 10000, (void *) 1, tskIDLE_PRIORITY, &xHandle);
    configASSERT(xHandle);

    xTaskCreatePinnedToCore(retriever_task, "retrieverTask", 40000, (void *) 1, tskIDLE_PRIORITY, &xHandle, 1);
    configASSERT(xHandle);
  #endif
} 

bool 
PageLocs::retrieve_asap(int16_t itemref_index) 
{
  StateQueueData state_queue_data;
  if (state_task.retriever_is_iddle()) {
    state_queue_data.req = StateReq::START_DOCUMENT;
    state_queue_data.itemref_count = item_count;
    state_queue_data.itemref_index = itemref_index;
    LOG_D("retrieve_asap: Sending START_DOCUMENT");
    QUEUE_SEND(state_queue, state_queue_data, 0);
  }
  state_queue_data.req = StateReq::GET_ASAP;
  state_queue_data.itemref_index = itemref_index;
  LOG_D("retrieve_asap: Sending GET_ASAP");
  QUEUE_SEND(state_queue, state_queue_data, 0);

  MgrQueueData mgr_queue_data;
  QUEUE_RECEIVE(mgr_queue, mgr_queue_data, portMAX_DELAY);
  LOG_D("-> %s <-", mgr_queue_data.req == MgrReq::ASAP_READY ? "ASAP_READY" : "ERROR!!!");

  return true;
}

void 
PageLocs::start_new_document(int16_t count) 
{ 
  item_count = count;

  if (!state_task.retriever_is_iddle()) {
    StateQueueData state_queue_data;
    state_queue_data.req = StateReq::STOP;
    QUEUE_SEND(state_queue, state_queue_data, 0);
  }
}


PageLocs::PagesMap::iterator 
PageLocs::check_and_find(const PageId & page_id) 
{
  std::scoped_lock guard(mutex);
  PagesMap::iterator it = pages_map.find(page_id);
  if (!completed && (it == pages_map.end())) {
    if (retrieve_asap(page_id.itemref_index)) it = pages_map.find(page_id);
  }
  return it;
}

const PageLocs::PageId * 
PageLocs::get_next_page_id(const PageId & page_id, int16_t count) 
{
  std::scoped_lock guard(mutex);
  PagesMap::iterator it = check_and_find(page_id);
  if (it == pages_map.end()) {
    it = check_and_find(PageId(0,0));
  }
  else {
    PageId id = page_id;

    for (int16_t cptr = count; cptr > 0; cptr--) {
      PagesMap::iterator prev = it;
      id.offset += it->second.size;
      it = pages_map.find(id);
      if (it == pages_map.end()) {
        id.itemref_index += 1; id.offset = 0;
        it = check_and_find(id);
        if (it == pages_map.end()) {
          it = (count > 1) ? prev : check_and_find(PageId(0,0));
          break;
        }
      }
    }
  }
  return (it == pages_map.end()) ? nullptr : &it->first;
}

const PageLocs::PageId * 
PageLocs::get_prev_page_id(const PageId & page_id, int count) 
{
  std::scoped_lock guard(mutex);
  PagesMap::iterator it = check_and_find(page_id);
  if (it == pages_map.end()) {
    it = check_and_find(PageId(0,0));
  }
  else {
    PageId id = page_id;
      
    for (int16_t cptr = count; cptr > 0; cptr--) {
      if (id.offset == 0) {
        if (id.itemref_index == 0) {
          if (count == 1) id.itemref_index = item_count - 1;
          else break;
        }
        else id.itemref_index--;
        if (items_set.find(id.itemref_index) == items_set.end()) retrieve_asap(id.itemref_index);
      }
      
      if (it == pages_map.begin()) it = pages_map.end();
      it--;
      id = it->first;
    }
  }
  return (it == pages_map.end()) ? nullptr : &it->first;
}

const PageLocs::PageId * 
PageLocs::get_page_id(const PageId & page_id) 
{
  std::scoped_lock guard(mutex);
  PagesMap::iterator it = check_and_find(PageId(page_id.itemref_index, 0));
  PagesMap::iterator res  = pages_map.end();
  while ((it != pages_map.end()) && (it->first.itemref_index == page_id.itemref_index)) {
    if ((it->first.offset <= page_id.offset) && ((it->first.offset + it->second.size) > page_id.offset)) { res = it; break; }
    it++;
  }
  return (res == pages_map.end()) ? nullptr : &res->first ;
}

void
PageLocs::computation_completed()
{
  std::scoped_lock guard(mutex);
  if (!completed) {
    int16_t page_nbr = 0;
    for (auto& entry : pages_map) {
      entry.second.page_number = page_nbr++;
    }
    completed = true;
  }
}

void
PageLocs::show()
{
  std::cout << "----- Page Locations -----" << std::endl;
  for (auto& entry : pages_map) {
    std::cout << " idx: " << entry.first.itemref_index
              << " off: " << entry.first.offset 
              << " siz: " << entry.second.size
              << " pg: "  << entry.second.page_number << std::endl;
  }
  std::cout << "----- End Page Locations -----" << std::endl;
}
