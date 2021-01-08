#define __PAGE_LOCS__ 1
#include "models/page_locs.hpp"
#include "models/config.hpp"
#include "controllers/event_mgr.hpp"

#include "viewers/book_viewer.hpp"
#include "viewers/page.hpp"
#include "logging.hpp"

#include "stb_image.h"

#include <esp_pthread.h>

enum class MgrReq : int8_t { ASAP_READY, STOPPED };

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

  #include <chrono>

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
  //SemaphoreHandle_t PageLocs::mutex = nullptr;
  //StaticSemaphore_t PageLocs::mutex_buffer;
#endif


// Both Linux and ESP32 algorithm versions must be synchronized by hand. As std::thread is not 
// close enough to the functionalities offered by FreeRTOS (Stack size control, core
// selection), no choice to use differents means on both platform.
#if 1 //EPUB_LINUX_BUILD
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
            retriever_iddle                   = false;
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
          retriever_iddle                   = false;
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
            retriever_iddle                   = false;
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
              if (bitset != nullptr) {
                delete [] bitset;
                bitset = nullptr;
              }
              mgr_queue_data.req = MgrReq::STOPPED;
              QUEUE_SEND(mgr_queue, mgr_queue_data, 0);
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
                itemref_count    = -1;
                retriever_iddle  = true;
                forget_retrieval = true;
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
                  retriever_iddle                   = false;
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

      inline bool   retriever_is_iddle() { return retriever_iddle;  } 
      inline bool forgetting_retrieval() { return forget_retrieval; }

  } state_task;

  inline bool   retriever_is_iddle() { return state_task.retriever_is_iddle();   } 
  inline bool forgetting_retrieval() { return state_task.forgetting_retrieval(); }

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

            LOG_D("Retrieving itemref --> %d <--", retrieve_queue_data.itemref_index);

            if (!page_locs.build_page_locs(retrieve_queue_data.itemref_index)) {
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
#else    

  static bool retriever_iddle          = true;

  static int16_t   itemref_count       = -1;      // Number of items in the document
  static int16_t   waiting_for_itemref = -1;      // Current item being processed by retrieval task
  static int16_t   next_itemref_to_get = -1;      // Non prioritize item to get next
  static int16_t   asap_itemref        = -1;      // Prioritize item to get next
  static uint8_t * bitset              = nullptr; // Set of all items processed so far
  static uint8_t   bitset_size         = 0;       // bitset byte length
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
  static void request_next_item(int16_t itemref,
                                bool already_sent_to_mgr = false)
  {
    static constexpr const char * TAG = "StateTask";
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
        retriever_iddle                   = false;
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
      retriever_iddle                   = false;
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
        retriever_iddle                   = false;
        LOG_D("Sent RETRIEVE_ITEM to Retriever");
      } else {
        page_locs.computation_completed();
        retriever_iddle = true;
      }
    }
  }

  static void state_task(void * param) 
  {
    static constexpr const char * TAG = "StateTask";
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
          if (bitset != nullptr) {
            delete [] bitset;
            bitset = nullptr;
          }
          mgr_queue_data.req = MgrReq::STOPPED;
          QUEUE_SEND(mgr_queue, mgr_queue_data, 0);
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
            itemref_count    = -1;
            retriever_iddle  = true;
            forget_retrieval = true;
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
              retriever_iddle                   = false;
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

  inline bool   retriever_is_iddle() { return retriever_iddle;  } 
  inline bool forgetting_retrieval() { return forget_retrieval; }

  static void retriever_task(void * param) 
  {
    static constexpr const char * TAG = "RetrieverTask";

    RetrieveQueueData retrieve_queue_data;
    StateQueueData state_queue_data;

    for (;;) {
      if (QUEUE_RECEIVE(retrieve_queue, retrieve_queue_data, portMAX_DELAY) == -1) {
        LOG_E("Receive error: %d: %s", errno, strerror(errno));
      }
      else {
        LOG_D("-> %s <-", (retrieve_queue_data.req == RetrieveReq::GET_ASAP) ? "GET_ASAP" : "RETRIEVE_ITEM");

        LOG_D("Retrieving itemref --> %d <--", retrieve_queue_data.itemref_index);

        if (!page_locs.build_page_locs(retrieve_queue_data.itemref_index)) {
          // Unable to retrieve pages location for the requested index. Send back
          // a negative value to indicate the issue to the state task
          state_queue_data.itemref_index = -(retrieve_queue_data.itemref_index + 1);
        }
        else {
          state_queue_data.itemref_index = retrieve_queue_data.itemref_index;
        }

        state_queue_data.req = 
          (retrieve_queue_data.req == RetrieveReq::GET_ASAP) ? 
            StateReq::ASAP_READY : StateReq::ITEM_READY;

        QUEUE_SEND(state_queue, state_queue_data, 0);
        LOG_D("Sent %s to State", (state_queue_data.req == StateReq::ASAP_READY) ? "ASAP_READY" : "ITEM_READY");
      }
    }
  }

#endif

static esp_pthread_cfg_t create_config(const char *name, int core_id, int stack, int prio)
{
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = name;
    cfg.pin_to_core = core_id;
    cfg.stack_size = stack;
    cfg.prio = prio;
    return cfg;
}

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

    auto cfg = create_config("retrieverTask", 0, 40 * 1024, 5);
    cfg.inherit_cfg = true;
    esp_pthread_set_cfg(&cfg);
    retriever_thread = std::thread(retriever_task);
    
    cfg = create_config("stateTask", 0, 10 * 1024, 5);
    cfg.inherit_cfg = true;
    esp_pthread_set_cfg(&cfg);
    state_thread = std::thread(state_task);

    #if 0
      TaskHandle_t xHandle = NULL;

      xTaskCreate(state_task, "stateTask", 10000, (void *) 1, tskIDLE_PRIORITY, &xHandle);
      configASSERT(xHandle);

      xTaskCreatePinnedToCore(retriever_task, "retrieverTask", 40000, (void *) 1, tskIDLE_PRIORITY, &xHandle, 1);
      configASSERT(xHandle);
    #endif
  #endif

} 

bool 
PageLocs::page_locs_end_page(Page::Format & fmt)
{
  bool res = true;
  if (!page.is_empty()) {

    PageLocs::PageId   page_id   = PageLocs::PageId(item_info.itemref_index, start_of_page_offset);
    PageLocs::PageInfo page_info = PageLocs::PageInfo(current_offset - start_of_page_offset, -1);
    
    res = page_locs.insert(page_id, page_info);

    // Gives the chance to book_viewer to show a page if required
    book_viewer.get_mutex().unlock();
    book_viewer.get_mutex().lock();

    // LOG_D("Page %d, offset: %d, size: %d", epub.get_page_count(), loc.offset, loc.size);
 
    SET_PAGE_TO_SHOW(epub.get_page_count()) // Debugging stuff
  }

  start_of_page_offset = current_offset;

  page.start(fmt);

  return res;
}

bool
PageLocs::page_locs_recurse(pugi::xml_node node, Page::Format fmt)
{
  if (node == nullptr) return false;
  
  const char * name;
  const char * str = nullptr;
  std::string image_filename;

  image_filename.clear();

  Elements::iterator element_it = elements.end();
  
  bool named_element = *(name = node.name()) != 0;

  if (named_element) { // A name is attached to the node.

    fmt.display = CSS::Display::INLINE;

    if ((element_it = elements.find(name)) != elements.end()) {

      //LOG_D("==> %10s [%5d] %4d", name, current_offset, page.get_pos_y());

      switch (element_it->second) {
        case Element::BODY:
        case Element::SPAN:
        case Element::A:
          break;
      #if NO_IMAGE
        case IMG:
        case IMAGE:
          break;
      #else
        case Element::IMG: 
          if (show_images) {
            xml_attribute attr = node.attribute("src");
            if (attr != nullptr) image_filename = attr.value();
            else current_offset++;
          }
          else {
            xml_attribute attr = node.attribute("alt");
            if (attr != nullptr) str = attr.value();
            else current_offset++;
          }
          break;
        case Element::IMAGE: 
          if (show_images) {
            xml_attribute attr = node.attribute("xlink:href");
            if (attr != nullptr) image_filename = attr.value();
            else current_offset++;
          }
          break;
      #endif
        case Element::PRE:
          fmt.pre     = start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::LI:
        case Element::DIV:
        case Element::BLOCKQUOTE:
        case Element::P:
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::BREAK:
          SHOW_LOCATION("Page Break");
          if (!page.line_break(fmt)) {
            if (!page_locs_end_page(fmt)) return false;
            SHOW_LOCATION("Page Break");
            page.line_break(fmt);
          }
          current_offset++;
          break;
        case Element::B: {
            Fonts::FaceStyle style = fmt.font_style;
            if      (style == Fonts::FaceStyle::NORMAL) style = Fonts::FaceStyle::BOLD;
            else if (style == Fonts::FaceStyle::ITALIC) style = Fonts::FaceStyle::BOLD_ITALIC;
            page.reset_font_index(fmt, style);
          }
          break;
        case Element::I:
        case Element::EM: {
            Fonts::FaceStyle style = fmt.font_style;
            if      (style == Fonts::FaceStyle::NORMAL) style = Fonts::FaceStyle::ITALIC;
            else if (style == Fonts::FaceStyle::BOLD  ) style = Fonts::FaceStyle::BOLD_ITALIC;
            page.reset_font_index(fmt, style);
          }
          break;
        case Element::H1:
          fmt.font_size          = 1.25 * fmt.font_size;
          fmt.line_height_factor = 1.25 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H2:
          fmt.font_size          = 1.1 * fmt.font_size;
          fmt.line_height_factor = 1.1 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H3:
          fmt.font_size          = 1.05 * fmt.font_size;
          fmt.line_height_factor = 1.05 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H4:
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H5:
          fmt.font_size          = 0.8 * fmt.font_size;
          fmt.line_height_factor = 0.8 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H6:
          fmt.font_size          = 0.7 * fmt.font_size;
          fmt.line_height_factor = 0.7 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
      }
    }

    xml_attribute attr = node.attribute("style");
    CSS::Properties * element_properties = nullptr;
    if (attr) {
      const char * buffer = attr.value();
      const char * end    = buffer + strlen(buffer);
      element_properties  = CSS::parse_properties(&buffer, end, buffer);
    }

    page.adjust_format(node, fmt, element_properties, item_info.css); // Adjust format from element attributes

    if (element_properties) {
      CSS::clear_properties(element_properties);
      element_properties = nullptr;
    }

    if (fmt.display == CSS::Display::BLOCK) {
      if (page.some_data_waiting()) {
        SHOW_LOCATION("End Paragraph 3");
        if (!page.end_paragraph(fmt)) {
          if (!page_locs_end_page(fmt)) return false;

          if (page.some_data_waiting()) {
            SHOW_LOCATION("End Paragraph 4");
            page.end_paragraph(fmt);
          }
        }
      }
      SHOW_LOCATION("New Paragraph 4");
      if (!page.new_paragraph(fmt)) {
        if (!page_locs_end_page(fmt)) return false;
        SHOW_LOCATION("New Paragraph 5");
        page.new_paragraph(fmt);
      }
    } 

    // if ((fmt.display == CSS::Display::BLOCK) && page.some_data_waiting()) {
    //   SHOW_LOCATION("End Paragraph 1");
    //   if (!page.end_paragraph(fmt)) {
    //     page_locs_end_page(fmt);
    //     SHOW_LOCATION("End Paragraph 2");
    //     page.end_paragraph(fmt);
    //   }
    // }
    // if ((current_offset == start_of_page_offset) && start_of_paragraph) {
    //   SHOW_LOCATION("New Paragraph 1");
    //   if (!page.new_paragraph(fmt)) {
    //     page_locs_end_page(fmt);
    //     SHOW_LOCATION("New Paragraph 2");
    //     page.new_paragraph(fmt);
    //   }
    //   start_of_paragraph = false;
    // }
  }
  else {
    //This is a node inside a named node. It is contaning some text to show.
    str = fmt.pre ? node.text().get() : node.value();
  }

  if (show_images && !image_filename.empty()) {
    Page::Image image;
    if (page.get_image(image_filename, image)) {
      if (!page.add_image(image, fmt)) {
        if (!page_locs_end_page(fmt)) return false;
        if (start_of_paragraph) {
          SHOW_LOCATION("New Paragraph 3");
          page.new_paragraph(fmt);
          start_of_paragraph = false;
        }
        page.add_image(image, fmt);
      }
      stbi_image_free((void *) image.bitmap);
    }
    current_offset++;
  }

  if (str) {
    SHOW_LOCATION("->");
    while (*str) {
      if (uint8_t(*str) <= ' ') {
        if (*str == ' ') {
          fmt.trim = !fmt.pre;
          if (!page.add_char(str, fmt)) {
            if (!page_locs_end_page(fmt)) return false;
            if (start_of_paragraph) {
              SHOW_LOCATION("New Paragraph 6");
              page.new_paragraph(fmt, false);
            }
            else {
              SHOW_LOCATION("New Paragraph 7");
              page.new_paragraph(fmt, true);
            }
          }
        }
        else if (fmt.pre && (*str == '\n')) {
          page.line_break(fmt, 30);
        }
        str++;
        current_offset++; // Not an UTF-8, so it's ok...
      }
      else {
        const char * w = str;
        int32_t count = 0;
        while (uint8_t(*str) > ' ') { str++; count++; }
        std::string word;
        word.assign(w, count);
        if (!page.add_word(word.c_str(), fmt)) {
          if (!page_locs_end_page(fmt)) return false;
          if (start_of_paragraph) {
            SHOW_LOCATION("New Paragraph 8");
            page.new_paragraph(fmt, false);
          }
          else {
            SHOW_LOCATION("New Paragraph 9");
            page.new_paragraph(fmt, true);
          }
          page.add_word(word.c_str(), fmt);
        }
        current_offset += count;
        start_of_paragraph = false;
      }
    } 
  }

  if (named_element) {

    xml_node sub = node.first_child();
    while (sub) {
      if (!page_locs_recurse(sub, fmt)) return false;
      sub = sub.next_sibling();
    }

    if (fmt.display == CSS::Display::BLOCK) {
      if ((current_offset != start_of_page_offset) || page.some_data_waiting()) {
        SHOW_LOCATION("End Paragraph 5");
        if (!page.end_paragraph(fmt)) {
          if (!page_locs_end_page(fmt)) return false;
          if (page.some_data_waiting()) {
            SHOW_LOCATION("End Paragraph 6");
            page.end_paragraph(fmt);
          }
        }
      }
      start_of_paragraph = false;
    } 

    // In case that we are at the end of an html file and there remains
    // characters in the page pipeline, we call end_paragraph() to get them out on the page...
    if ((element_it != elements.end()) && (element_it->second == Element::BODY)) {
      SHOW_LOCATION("End Paragraph 7");
      if (!page.end_paragraph(fmt)) {
        if (!page_locs_end_page(fmt)) return false;
        if (page.some_data_waiting()) {
          SHOW_LOCATION("End Paragraph 8");
          page.end_paragraph(fmt);
        }
      }
    }
  } 
  return true;
}

bool
PageLocs::build_page_locs(int16_t itemref_index)
{
  std::scoped_lock guard(book_viewer.get_mutex());

  TTF * font  = fonts.get(0, 10);
  page_bottom = font->get_line_height() + (font->get_line_height() >> 1);
  
  page.set_compute_mode(Page::ComputeMode::LOCATION);

  int8_t images_are_shown;
  config.get(Config::Ident::SHOW_IMAGES, &images_are_shown);
  show_images = images_are_shown == 1;

  bool done = false;

  if (epub.get_item_at_index(itemref_index, item_info)) {

    int16_t idx;

    if ((idx = fonts.get_index("Fontbase", Fonts::FaceStyle::NORMAL)) == -1) {
      idx = 1;
    }
    
    int8_t font_size;
    config.get(Config::Ident::FONT_SIZE, &font_size);

    Page::Format fmt = {
      .line_height_factor = 0.9,
      .font_index         = idx,
      .font_size          = font_size,
      .indent             = 0,
      .margin_left        = 0,
      .margin_right       = 0,
      .margin_top         = 0,
      .margin_bottom      = 0,
      .screen_left        = 10,
      .screen_right       = 10,
      .screen_top         = 10,
      .screen_bottom      = page_bottom,
      .width              = 0,
      .height             = 0,
      .trim               = true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

    while (!done) {

      current_offset       = 0;
      start_of_page_offset = 0;
      xml_node node = item_info.xml_doc.child("html");

      if (node && 
         (node = node.child("body"))) {

        page.start(fmt);

        if (!page_locs_recurse(node, fmt)) {
          LOG_D("html parsing issue or aborted by Mgr");
          break;
        }

        if (page.some_data_waiting()) page.end_paragraph(fmt);
      }
      else {
        LOG_D("No <body>");
        break;
      }

      page_locs_end_page(fmt);

      done = true;
    }
  }

  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  // epub.clear_item_data(item_info);

  if (item_info.css != nullptr) {
    delete item_info.css;
    item_info.css = nullptr;
  }

  return done;
}

volatile bool relax = false;

bool 
PageLocs::retrieve_asap(int16_t itemref_index) 
{
  StateQueueData state_queue_data;
  state_queue_data.req = StateReq::GET_ASAP;
  state_queue_data.itemref_index = itemref_index;
  LOG_D("retrieve_asap: Sending GET_ASAP");
  QUEUE_SEND(state_queue, state_queue_data, 0);

  relax = true;
  MgrQueueData mgr_queue_data;
  QUEUE_RECEIVE(mgr_queue, mgr_queue_data, portMAX_DELAY);
  LOG_D("-> %s <-", mgr_queue_data.req == MgrReq::ASAP_READY ? "ASAP_READY" : "ERROR!!!");
  relax = false;

  return true;
}

void
PageLocs::stop_document()
{
  StateQueueData state_queue_data;

  LOG_D("start_new_document: Sending STOP");
  state_queue_data.req = StateReq::STOP;
  QUEUE_SEND(state_queue, state_queue_data, 0);

  MgrQueueData mgr_queue_data;
  QUEUE_RECEIVE(mgr_queue, mgr_queue_data, portMAX_DELAY);
  LOG_D("-> %s <-", (mgr_queue_data.req == MgrReq::STOPPED) ? "STOPPED" : "ERROR!!!");
}

void 
PageLocs::start_new_document(int16_t count, int16_t itemref_index) 
{ 
  item_count = count;
  StateQueueData state_queue_data;

  if (!retriever_is_iddle()) stop_document();

  state_queue_data.req           = StateReq::START_DOCUMENT;
  state_queue_data.itemref_count = item_count;
  state_queue_data.itemref_index = itemref_index;
  LOG_D("start_new_document: Sending START_DOCUMENT");
  QUEUE_SEND(state_queue, state_queue_data, 0);

  event_mgr.set_stay_on(true);

  clear();
}


PageLocs::PagesMap::iterator 
PageLocs::check_and_find(const PageId & page_id) 
{
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

bool 
PageLocs::insert(PageId & id, PageInfo & info) 
{
  if (!forgetting_retrieval()) {
    while (true) {
      if (relax) {
        // The page_locs class is still in control of the mutex, but is waiting
        // for the completion of an GET_ASAP item. As such, it is safe to insert
        // a new page info in the list.
        LOG_D("Relaxed page info insert...");
        pages_map.insert(std::make_pair(id, info));
        items_set.insert(id.itemref_index);
        break;
      }
      else {
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
    event_mgr.set_stay_on(false);
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
