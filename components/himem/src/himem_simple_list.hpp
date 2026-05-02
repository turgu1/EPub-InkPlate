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
using HimemSimpleList = SimpleList<T, HimemPool<T>>;

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
