#pragma once

#include "global.hpp"
#include "himem.hpp"

#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <forward_list>
#include <string>

using CharPoolPtr = HimemUniquePtr<class CharPool>;
class CharPool {
private:
  static constexpr char const *TAG = "CharPool";
  static const uint16_t POOL_SIZE  = 4096;

  using PoolPtr = HimemUniquePtr<char[]>;

  std::forward_list<PoolPtr> pool_list;

  char *current{nullptr};
  uint16_t current_idx{0};
  uint32_t total_allocated{0};

  CharPool() = default;

public:
  ~CharPool() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make() { return makeUniqueHimem<CharPool>(); }

  uint32_t get_total_allocated() { return total_allocated; }

  char *allocate(uint16_t size) {
    if ((current == nullptr) || ((current_idx + size) >= POOL_SIZE)) {
      LOG_D("New pool allocation.");
      auto pool = makeUniqueHimem<char[]>(POOL_SIZE);
      if (pool) {
        pool_list.push_front(std::move(pool));
        current     = pool_list.front().get();
        current_idx = 0;
      } else {
        return nullptr;
      }
    }

    char *tmp = &((current)[current_idx]);

    current_idx += size;
    total_allocated += size;

    return tmp;
  }

  char *set(std::string str) {
    char *p = this->allocate(str.length() + 1);
    memcpy(p, str.c_str(), str.length() + 1);
    return p;
  }
};