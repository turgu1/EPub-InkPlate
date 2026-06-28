// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

// ---------------------------------------------------------------------------
// HimemSimpleList<T>
//
// Drop-in for SimpleList<T> whose node storage is pooled in PSRAM via
// HimemPool<T>.  Nodes are allocated from bulk PSRAM blocks (default 20 per
// block) rather than one malloc per node, giving both cache-friendly layout
// and reduced allocator overhead.
//
// On ESP32 targets every block allocation hits SPIRAM; on Linux/host builds
// it falls back to the normal heap so that unit-tests run unmodified.
//
// Usage:
//   HimemSimpleList<int> list;
//   list.pushBack(42);           // node slot from embedded HimemPool
//   list.clear();                // slots recycled back to the pool
//
// HimemUniqueSimpleList<T> / makeUniqueHimemSimpleList<T>(...) puts the
// *list object itself* in PSRAM as well (nodes are always pooled in PSRAM).
//
// NOTE: This header is separate from himem.hpp to avoid a circular-include
// cycle: himem_pool.hpp (which provides HimemPool<T>) already includes
// himem.hpp.  Include this header wherever HimemSimpleList is needed.
// ---------------------------------------------------------------------------

// himem_pool.hpp pulls in himem.hpp (for PsramAllocator / HimemUniquePtr),
// so the correct include order is: himem_pool first, simple_list second.
#include "himem_pool.hpp"
#include "simple_list.hpp"

template <typename T>
class HimemSimpleList : public SimpleList<T, HimemPool<T>> {
private:
  using Base = SimpleList<T, HimemPool<T>>;

public:
  using typename Base::ConstIterator;
  using typename Base::Iterator;
  using iterator       = Iterator;
  using const_iterator = ConstIterator;

  using Base::Base;

  HimemSimpleList() = default;

  HimemSimpleList(const HimemSimpleList &)                         = default;
  HimemSimpleList(HimemSimpleList &&) noexcept                     = default;
  auto operator=(const HimemSimpleList &) -> HimemSimpleList &     = default;
  auto operator=(HimemSimpleList &&) noexcept -> HimemSimpleList & = default;
  ~HimemSimpleList()                                               = default;

  void push_front(const T &value) noexcept { this->pushFront(value); }
  void push_front(T &&value) noexcept { this->pushFront(std::move(value)); }

  template <typename... Args>
  auto emplace_front(Args &&...args) noexcept -> T * {
    return this->emplaceFront(std::forward<Args>(args)...);
  }

  void pop_front() noexcept { this->popFront(); }

  void push_back(const T &value) noexcept { this->pushBack(value); }
  void push_back(T &&value) noexcept { this->pushBack(std::move(value)); }

  template <typename... Args>
  auto emplace_back(Args &&...args) noexcept -> T * {
    return this->emplaceBack(std::forward<Args>(args)...);
  }

  void pop_back() noexcept { this->removeLast(); }

  void reverse() noexcept {
    HimemSimpleList<T> rev;
    while (!this->empty()) {
      rev.pushFront(std::move(this->front()));
      this->popFront();
    }
    this->merge(rev);
  }
};

template <typename T>
using HimemUniqueSimpleList = HimemUniquePtr<HimemSimpleList<T>>;

// Factory: constructs a HimemSimpleList<T> in PSRAM and returns a unique owner.
template <typename T, typename... Args>
[[nodiscard]] auto makeUniqueHimemSimpleList(Args &&...args) -> HimemUniqueSimpleList<T> {
  using List = HimemSimpleList<T>;

  PsramAllocator<List> listAlloc;
  List *obj = listAlloc.allocate(1);
  ::new (obj) List(std::forward<Args>(args)...);

  return HimemUniqueSimpleList<T>{obj};
}
