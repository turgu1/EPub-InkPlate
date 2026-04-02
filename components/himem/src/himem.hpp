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
//   himem_shared_ptr<T>          — alias for std::shared_ptr<T>
//   make_shared_himem<T>(...)    — allocates T (or T[], unbounded) in PSRAM
//
//   himem_unique_ptr<T>          — std::unique_ptr<T, PsramDeleter<T>>
//   make_unique_himem<T>(...)    — allocates T (or T[], unbounded) in PSRAM
//
//   himem_string                 — std::basic_string with PsramAllocator
//   himem_vector<T>              — std::vector<T>  with PsramAllocator
// ---------------------------------------------------------------------------

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
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

  [[nodiscard]] T *allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_array_new_length{};

    void *ptr = HIMEM_MALLOC(n * sizeof(T));
    if (!ptr) throw std::bad_alloc{};

    return static_cast<T *>(ptr);
  }

  void deallocate(T *ptr, std::size_t /* n */) noexcept { HIMEM_FREE(ptr); }

  // ---- equality ------------------------------------------------------------
  // All instances allocate from the same heap ⟹ always equal.
  template <typename U>
  [[nodiscard]] constexpr bool operator==(const PsramAllocator<U> &) const noexcept {
    return true;
  }
};

// ===========================================================================
// PsramDeleter<T> / PsramDeleter<T[]>
//
// Custom deleters used by himem_unique_ptr.  They call the appropriate
// destructors before releasing memory back to the PSRAM heap.
// ===========================================================================

// Primary template — scalar (non-array) objects.
template <typename T>
struct PsramDeleter {
  PsramDeleter() = default;
  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  PsramDeleter(const PsramDeleter<U> &) noexcept {}

  void operator()(T *ptr) const noexcept {
    if (ptr) {
      std::destroy_at(ptr); // calls ptr->~T()
      HIMEM_FREE(ptr);
    }
  }
};

// Partial specialisation — arrays (used by himem_unique_ptr<T[]>).
// The element count must be stored so that non-trivially destructible
// element types receive proper destruction.
template <typename T>
struct PsramDeleter<T[]> {
  std::size_t count{0};

  void operator()(T *ptr) const noexcept {
    if (ptr) {
      std::destroy_n(ptr, count); // no-op for trivially-destructible T
      HIMEM_FREE(ptr);
    }
  }
};

// ===========================================================================
// himem_shared_ptr / make_shared_himem
//
// std::allocate_shared places the control-block and the managed object in
// a single allocation, so the entire shared_ptr machinery lives in PSRAM.
// ===========================================================================

template <typename T>
using himem_shared_ptr = std::shared_ptr<T>;

// Single-object overload
template <typename T, typename... Args>
  requires(!std::is_array_v<T>)
[[nodiscard]] himem_shared_ptr<T> make_shared_himem(Args &&...args) {
  return std::allocate_shared<T>(PsramAllocator<T>{}, std::forward<Args>(args)...);
}

// Unbounded-array overload  (e.g. make_shared_himem<int[]>(n))
template <typename T>
  requires(std::is_unbounded_array_v<T>)
[[nodiscard]] himem_shared_ptr<T> make_shared_himem(std::size_t n) {
  using Elem = std::remove_extent_t<T>;
  return std::allocate_shared<T>(PsramAllocator<Elem>{}, n);
}

// Bounded-array form is deleted, mirroring std::make_shared behaviour.
template <typename T, typename... Args>
  requires(std::is_bounded_array_v<T>)
void make_shared_himem(Args &&...) = delete;

// ===========================================================================
// himem_unique_ptr / make_unique_himem
// ===========================================================================

template <typename T>
using himem_unique_ptr = std::unique_ptr<T, PsramDeleter<T>>;

// Tag type for the sized single-object overload.
// Pass himem_sized(n) to make_unique_himem<T> to allocate n bytes instead of sizeof(T).
// Useful when T is followed by a variable-length trailing payload in the same allocation.
struct himem_sized_t {
  std::size_t bytes;
};
[[nodiscard]] inline himem_sized_t himem_sized(std::size_t n) noexcept { return {n}; }

// Sized single-object overload  (e.g. make_unique_himem<T>(himem_sized(n)))
// Allocates exactly sz.bytes bytes, default-constructs T in that buffer.
template <typename T>
  requires(!std::is_array_v<T>)
[[nodiscard]] himem_unique_ptr<T> make_unique_himem(himem_sized_t sz) {
  void *mem = HIMEM_MALLOC(sz.bytes);
  if (!mem) throw std::bad_alloc{};

  T *obj = nullptr;
  try {
    obj = ::new (mem) T();
  } catch (...) {
    HIMEM_FREE(mem);
    throw;
  }

  return himem_unique_ptr<T>{obj};
}

// Single-object overload
template <typename T, typename... Args>
  requires(!std::is_array_v<T>)
[[nodiscard]] himem_unique_ptr<T> make_unique_himem(Args &&...args) {
  void *mem = HIMEM_MALLOC(sizeof(T));
  if (!mem) throw std::bad_alloc{};

  T *obj = nullptr;
  try {
    obj = ::new (mem) T(std::forward<Args>(args)...);
  } catch (...) {
    HIMEM_FREE(mem);
    throw;
  }

  return himem_unique_ptr<T>{obj};
}

// Unbounded-array overload  (e.g. make_unique_himem<int[]>(n))
template <typename T>
  requires(std::is_unbounded_array_v<T>)
[[nodiscard]] himem_unique_ptr<T> make_unique_himem(std::size_t n) {
  using Elem = std::remove_extent_t<T>;

  void *mem = HIMEM_MALLOC(n * sizeof(Elem));
  if (!mem) throw std::bad_alloc{};

  Elem *arr = static_cast<Elem *>(mem);
  try {
    std::uninitialized_value_construct_n(arr, n);
  } catch (...) {
    HIMEM_FREE(mem);
    throw;
  }

  PsramDeleter<T> del{n};
  return himem_unique_ptr<T>{arr, del};
}

// Bounded-array form is deleted, mirroring std::make_unique behaviour.
template <typename T, typename... Args>
  requires(std::is_bounded_array_v<T>)
void make_unique_himem(Args &&...) = delete;

// ===========================================================================
// himem_string
//
// Drop-in for std::string whose internal character buffer lives in PSRAM.
// Note: iterators and data() pointers point into SPIRAM — avoid caching them
// across modifications on multi-core targets without proper synchronisation.
// ===========================================================================

using himem_string = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

// ===========================================================================
// himem_vector<T>
//
// Drop-in for std::vector<T> whose backing storage lives in PSRAM.
// ===========================================================================

template <typename T>
using himem_vector = std::vector<T, PsramAllocator<T>>;

// ===========================================================================
// himem_unique_string
//
// A himem_string object whose own storage (in addition to its character
// buffer) also lives in PSRAM.  Ownership is expressed via unique_ptr.
// ===========================================================================

using himem_unique_string = himem_unique_ptr<himem_string>;

// Factory: constructs a himem_string in PSRAM and returns a unique owner.
// Accepts the same arguments as himem_string constructors, forwarded
// directly (e.g. a const char*, another himem_string, or nothing for empty).
template <typename... Args>
[[nodiscard]] himem_unique_string make_unique_himem_string(Args &&...args) {
  void *mem = HIMEM_MALLOC(sizeof(himem_string));
  if (!mem) throw std::bad_alloc{};

  himem_string *obj = nullptr;
  try {
    obj = ::new (mem) himem_string(std::forward<Args>(args)...);
  } catch (...) {
    HIMEM_FREE(mem);
    throw;
  }

  return himem_unique_string{obj};
}

// ===========================================================================
// himem_unique_vector<T>
//
// A himem_vector<T> object whose own storage (in addition to its element
// buffer) also lives in PSRAM.  Ownership is expressed via unique_ptr.
// ===========================================================================

template <typename T>
using himem_unique_vector = himem_unique_ptr<himem_vector<T>>;

// Factory: constructs a himem_vector<T> in PSRAM and returns a unique owner.
// Accepts the same arguments as himem_vector constructors (e.g. an initial
// size, or nothing for an empty vector).
template <typename T, typename... Args>
[[nodiscard]] himem_unique_vector<T> make_unique_himem_vector(Args &&...args) {
  using Vec = himem_vector<T>;

  void *mem = HIMEM_MALLOC(sizeof(Vec));
  if (!mem) throw std::bad_alloc{};

  Vec *obj = nullptr;
  try {
    obj = ::new (mem) Vec(std::forward<Args>(args)...);
  } catch (...) {
    HIMEM_FREE(mem);
    throw;
  }

  return himem_unique_vector<T>{obj};
}

// Clean up internal macros so they don't leak into translation units.
#undef HIMEM_MALLOC
#undef HIMEM_FREE
