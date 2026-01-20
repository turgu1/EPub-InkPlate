#pragma once

#include <forward_list>
#include <inttypes.h>
#include <stdlib.h>

class CharPool
{
  private:
    static constexpr char const * TAG = "CharPool";
    static const uint16_t POOL_SIZE = 4096;
    typedef char Pool[POOL_SIZE];

    std::forward_list<Pool *> pool_list;

    Pool *   current;
    uint16_t current_idx;
    uint32_t total_allocated;

  public:
    CharPool() : 
              current(nullptr), 
          current_idx(0),
      total_allocated(0) {}

   ~CharPool() { for (auto * pool : pool_list) free(pool); }

    uint32_t get_total_allocated() { return total_allocated; }
    char * allocate(uint16_t size) {
      if ((current == nullptr) || ((current_idx + size) >= POOL_SIZE)) {
        LOG_D("New pool allocation.");
        if ((current = (Pool *) malloc(sizeof(Pool))) != nullptr) {
          pool_list.push_front(current);
          current_idx = 0;
        }
        else {
          return nullptr;
        }
      }

      char * tmp = &(((char *)current)[current_idx]);
      
      current_idx     += size;
      total_allocated += size;

      return tmp;
    }

    char * set(std::string str) {
      char * p = this->allocate(str.length() + 1);
      memcpy(p, str.c_str(), str.length() + 1);
      return p;
    }
};