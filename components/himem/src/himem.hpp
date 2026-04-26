// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

// ---------------------------------------------------------------------------
// himem — PSRAM-backed allocator and smart-pointer / container helpers
//
// On EPUB_INKPLATE_BUILD targets every allocation hits SPIRAM via
// heap_caps_malloc(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT).
// On host/Linux builds the implementation silently falls back to the
// default heap so that unit-tests can run unmodified.
//
// Provided utilities
// ------------------
//   PsramAllocator<T>            — C++23 named-allocator requirement
//
//   HimemSharedPtr<T>          — alias for std::shared_ptr<T>
//   makeSharedHimem<T>(...)    — allocates T (or T[], unbounded) in PSRAM
//
//   HimemUniquePtr<T>          — std::unique_ptr<T, PsramDeleter<T>>
//   makeUniqueHimem<T>(...)    — allocates T (or T[], unbounded) in PSRAM
//
//   HimemString                 — std::basic_string with PsramAllocator
//   HimemVector<T>              — std::vector<T>  with PsramAllocator
//   HimemMap<K, V>              — std::map<K, V>  with PsramAllocator
//   HimemUnorderedMap<K, V>     — std::unordered_map<K, V> with PsramAllocator
// ---------------------------------------------------------------------------

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#if EPUB_INKPLATE_BUILD
  #include "esp_heap_caps.h"
  #define HIMEM_MALLOC(size_) heap_caps_malloc((size_), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
  #define HIMEM_FREE(ptr_) heap_caps_free(ptr_)
#else
  #define HIMEM_MALLOC(size_) std::malloc(size_)
  #define HIMEM_FREE(ptr_) std::free(ptr_)
#endif

// ===========================================================================
// PsramAllocator<T>
//
// Stateless allocator that satisfies the C++23 Allocator named requirement.
// Rebinding (required by e.g. std::allocate_shared internals) is supported
// through the templated copy constructor.
// ===========================================================================

template <typename T>
class PsramAllocator {
public:
  // ---- member types --------------------------------------------------------
  using value_type      = T;
  using size_type       = std::size_t;
  using difference_type = std::ptrdiff_t;

  // Hint to containers: instances can always be treated as equal (stateless).
  using is_always_equal = std::true_type;

  // Containers must move their allocator on move-assignment.
  using propagate_on_container_move_assignment = std::true_type;

  // ---- construction --------------------------------------------------------
  constexpr PsramAllocator() noexcept = default;

  template <typename U>
  constexpr PsramAllocator(const PsramAllocator<U> &) noexcept {} // rebind

  // ---- core interface ------------------------------------------------------

  [[nodiscard]] auto allocate(std::size_t n) -> T * {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      return nullptr; // match std::make_unique<T>(...) behaviour on allocation failure

    void *ptr = HIMEM_MALLOC(n * sizeof(T));
    if (!ptr) return nullptr; // match std::make_unique<T>(...) behaviour on allocation failure

    return static_cast<T *>(ptr);
  }

  auto deallocate(T *ptr, std::size_t /* n */) noexcept -> void { HIMEM_FREE(ptr); }

  // ---- equality ------------------------------------------------------------
  // All instances allocate from the same heap ⟹ always equal.
  template <typename U>
  [[nodiscard]] constexpr auto operator==(const PsramAllocator<U> &) const noexcept -> bool {
    return true;
  }
};

// ===========================================================================
// PsramDeleter<T> / PsramDeleter<T[]>
//
// Custom deleters used by HimemUniquePtr.  They call the appropriate
// destructors before releasing memory back to the PSRAM heap.
// ===========================================================================

// Primary template — scalar (non-array) objects.
template <typename T>
struct PsramDeleter {
  PsramDeleter() = default;
  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  PsramDeleter(const PsramDeleter<U> &) noexcept {}

  auto operator()(T *ptr) const noexcept -> void {
    if (ptr) {
      std::destroy_at(ptr); // calls ptr->~T()
      HIMEM_FREE(ptr);
    }
  }
};

// Partial specialisation — arrays (used by HimemUniquePtr<T[]>).
// The element count must be stored so that non-trivially destructible
// element types receive proper destruction.
template <typename T>
struct PsramDeleter<T[]> {
  std::size_t count{0};

  auto operator()(T *ptr) const noexcept -> void {
    if (ptr) {
      std::destroy_n(ptr, count); // no-op for trivially-destructible T
      HIMEM_FREE(ptr);
    }
  }
};

// ===========================================================================
// HimemSharedPtr / makeSharedHimem
//
// The managed object is placed in PSRAM via direct placement new.
// The shared_ptr control block lives on the default heap.
// ===========================================================================

template <typename T>
using HimemSharedPtr = std::shared_ptr<T>;

// Single-object overload
// Construction is done via placement new directly inside this function so that
// classes with private constructors can grant access with a simple friend
// declaration (same pattern as makeUniqueHimem).  The control block is placed
// on the default heap; the managed object itself lives in PSRAM.
template <typename T, typename... Args>
  requires(!std::is_array_v<T>)
[[nodiscard]] auto makeSharedHimem(Args &&...args) -> HimemSharedPtr<T> {
  void *mem = HIMEM_MALLOC(sizeof(T));
  if (!mem) return nullptr;
  T *obj = ::new (mem) T(std::forward<Args>(args)...);
  return HimemSharedPtr<T>{obj, PsramDeleter<T>{}};
}

// Unbounded-array overload  (e.g. makeSharedHimem<int[]>(n))
template <typename T>
  requires(std::is_unbounded_array_v<T>)
[[nodiscard]] auto makeSharedHimem(std::size_t n) -> HimemSharedPtr<T> {
  using Elem = std::remove_extent_t<T>;
  return std::allocate_shared<T>(PsramAllocator<Elem>{}, n);
}

// Bounded-array form is deleted, mirroring std::make_shared behaviour.
template <typename T, typename... Args>
  requires(std::is_bounded_array_v<T>)
auto makeSharedHimem(Args &&...) -> void = delete;

// ===========================================================================
// HimemUniquePtr / makeUniqueHimem
// ===========================================================================

template <typename T>
using HimemUniquePtr = std::unique_ptr<T, PsramDeleter<T>>;

// Tag type for the sized single-object overload.
// Pass himemSized(n) to makeUniqueHimem<T> to allocate n bytes instead of sizeof(T).
// Useful when T is followed by a variable-length trailing payload in the same allocation.
struct himemSizedT {
  std::size_t bytes;
};
[[nodiscard]] inline auto himemSized(std::size_t n) noexcept -> himemSizedT { return {n}; }

// Sized single-object overload  (e.g. makeUniqueHimem<T>(himemSized(n)))
// Allocates exactly sz.bytes bytes, default-constructs T in that buffer.
template <typename T>
  requires(!std::is_array_v<T>)
[[nodiscard]] auto makeUniqueHimem(himemSizedT sz) -> HimemUniquePtr<T> {
  void *mem = HIMEM_MALLOC(sz.bytes);
  if (!mem) return nullptr; // match std::make_unique<T>(...) behaviour on allocation failure

  T *obj = nullptr;
  obj    = ::new (mem) T();

  return HimemUniquePtr<T>{obj};
}

// Single-object overload
template <typename T, typename... Args>
  requires(!std::is_array_v<T>)
[[nodiscard]] auto makeUniqueHimem(Args &&...args) -> HimemUniquePtr<T> {
  void *mem = HIMEM_MALLOC(sizeof(T));
  if (!mem) return nullptr; // match std::make_unique<T>(...) behaviour on allocation failure

  T *obj = nullptr;
  obj    = ::new (mem) T(std::forward<Args>(args)...);

  return HimemUniquePtr<T>{obj};
}

// Unbounded-array overload  (e.g. makeUniqueHimem<int[]>(n))
template <typename T>
  requires(std::is_unbounded_array_v<T>)
[[nodiscard]] auto makeUniqueHimem(std::size_t n) -> HimemUniquePtr<T> {
  using Elem = std::remove_extent_t<T>;

  void *mem = HIMEM_MALLOC(n * sizeof(Elem));
  if (!mem) return nullptr; // match std::make_unique<T[]>(n) behaviour on allocation failure

  Elem *arr = static_cast<Elem *>(mem);
  std::uninitialized_value_construct_n(arr, n);

  PsramDeleter<T> del{n};
  return HimemUniquePtr<T>{arr, del};
}

// Bounded-array form is deleted, mirroring std::make_unique behaviour.
template <typename T, typename... Args>
  requires(std::is_bounded_array_v<T>)
auto makeUniqueHimem(Args &&...) -> void = delete;

// ===========================================================================
// HimemString
//
// Drop-in for std::string whose internal character buffer lives in PSRAM.
// Note: iterators and data() pointers point into SPIRAM — avoid caching them
// across modifications on multi-core targets without proper synchronisation.
// ===========================================================================

using HimemString = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

// ===========================================================================
// HimemVector<T>
//
// Drop-in for std::vector<T> whose backing storage lives in PSRAM.
// ===========================================================================

template <typename T>
class HimemVector : public std::vector<T, PsramAllocator<T>> {
public:
  using Base = std::vector<T, PsramAllocator<T>>;
  using Base::Base;

  HimemVector() = default;

  explicit HimemVector(const PsramAllocator<T> &alloc) : Base(alloc) {}
};

// ===========================================================================
// HimemUnorderedMap<K, V>
//
// Drop-in for std::unordered_map<K, V> whose backing storage lives in PSRAM.
// ===========================================================================

template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
using HimemUnorderedMap =
    std::unordered_map<Key, T, Hash, KeyEqual, PsramAllocator<std::pair<const Key, T>>>;

// ===========================================================================
// HimemMap<K, V>
//
// Drop-in for std::map<K, V> whose backing storage lives in PSRAM.
// ===========================================================================

template <typename Key, typename T, typename Compare = std::less<Key>>
using HimemMap = std::map<Key, T, Compare, PsramAllocator<std::pair<const Key, T>>>;

// ===========================================================================
// himemUniqueString
//
// A HimemString object whose own storage (in addition to its character
// buffer) also lives in PSRAM.  Ownership is expressed via unique_ptr.
// ===========================================================================

using himemUniqueString = HimemUniquePtr<HimemString>;

// Factory: constructs a HimemString in PSRAM and returns a unique owner.
// Accepts the same arguments as HimemString constructors, forwarded
// directly (e.g. a const char*, another HimemString, or nothing for empty).
template <typename... Args>
[[nodiscard]] auto makeUniqueHimemString(Args &&...args) -> himemUniqueString {
  void *mem = HIMEM_MALLOC(sizeof(HimemString));
  if (!mem) return nullptr; // match std::make_unique<T>(...) behaviour on allocation failure

  HimemString *obj = nullptr;
  obj              = ::new (mem) HimemString(std::forward<Args>(args)...);

  return himemUniqueString{obj};
}

// ===========================================================================
// himemUniqueVector<T>
//
// A HimemVector<T> object whose own storage (in addition to its element
// buffer) also lives in PSRAM.  Ownership is expressed via unique_ptr.
// ===========================================================================

template <typename T>
using himemUniqueVector = HimemUniquePtr<HimemVector<T>>;

// Factory: constructs a HimemVector<T> in PSRAM and returns a unique owner.
// Accepts the same arguments as HimemVector constructors (e.g. an initial
// size, or nothing for an empty vector).
template <typename T, typename... Args>
[[nodiscard]] auto makeUniqueHimemVector(Args &&...args) -> himemUniqueVector<T> {
  using Vec = HimemVector<T>;

  void *mem = HIMEM_MALLOC(sizeof(Vec));
  if (!mem) return nullptr; // match std::make_unique<T>(...) behaviour on allocation failure

  Vec *obj = nullptr;
  obj      = ::new (mem) Vec(std::forward<Args>(args)...);

  return himemUniqueVector<T>{obj};
}

// ===========================================================================
// HimemSimpleList<T>
//
// Drop-in for SimpleList<T> whose node storage lives in PSRAM.
// On ESP32 targets every node allocation hits SPIRAM; on Linux/host builds
// it falls back to the normal heap so that unit-tests run unmodified.
//
// Usage:
//   HimemSimpleList<int> list;
//   list.push_back(42);          // node in PSRAM
//   list.push_front(0);
//   list.emplace_back(99);
//
// himemUniqueSimpleList<T> / makeUniqueHimemSimpleList<T>(...) puts the
// *list object itself* in PSRAM as well (nodes are always in PSRAM).
// ===========================================================================

#include "simple_list.hpp"

template <typename T>
using HimemSimpleList = SimpleList<T, PsramAllocator<T>>;

template <typename T>
using himemUniqueSimpleList = HimemUniquePtr<HimemSimpleList<T>>;

// Factory: constructs a HimemSimpleList<T> in PSRAM and returns a unique owner.
template <typename T, typename... Args>
[[nodiscard]] auto makeUniqueHimemSimpleList(Args &&...args) -> himemUniqueSimpleList<T> {
  using List = HimemSimpleList<T>;

  // Re-define HIMEM_MALLOC/FREE locally since they've been #undef'd above
  // when this template is instantiated — use PsramAllocator directly instead.
  PsramAllocator<List> listAlloc;
  List *obj = listAlloc.allocate(1);
  ::new (obj) List(std::forward<Args>(args)...);

  return himemUniqueSimpleList<T>{obj};
}

// Clean up internal macros so they don't leak into translation units.
#undef HIMEM_MALLOC
#undef HIMEM_FREE
