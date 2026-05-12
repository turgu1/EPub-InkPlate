#pragma once

// ---------------------------------------------------------------------------
// CharPool — variable-size PSRAM character pool with free-list recycling
//
// Memory is acquired from PSRAM in BLOCK_SIZE-byte blocks. Each allocation
// request is rounded up to the next power-of-two size class (minimum 8 B,
// maximum BLOCK_SIZE = 4096 B). Freed slots are returned to a per-class
// free list and reused by the next matching request, making the pool
// suitable for std::basic_string whose buffer may be reallocated on growth.
//
// Hard constraint: a single allocation cannot exceed BLOCK_SIZE bytes
// (i.e. strings are limited to BLOCK_SIZE-1 = 4095 characters).
//
// Backward-compatible API:
//   char *allocate(std::size_t n)         — allocate at least n bytes
//   void  deallocate(char *ptr, size_t n) — return slot to free list
//   char *set(const std::string &str)     — copy string into pool
//
// New API:
//   CharPoolAllocator<T>            — STL allocator backed by a CharPool instance
//   PoolString                      — std::basic_string with CharPoolAllocator
//   PoolContext<Tag>                — associates a Tag type with one shared CharPool
//   StaticPoolAllocator<T, Tag>     — stateless allocator backed by PoolContext<Tag>
//
// StaticPoolAllocator usage pattern:
//   struct MyTag {};
//   using MyString = std::basic_string<char, std::char_traits<char>,
//                                      StaticPoolAllocator<char, MyTag>>;
//   // Before using MyString:
//   PoolContext<MyTag>::init(myPool.get());
//   MyString s = "hello";  // no allocator argument needed
//   // On teardown:
//   PoolContext<MyTag>::reset();
//   myPool.reset(); // frees all MyString buffers in one call
// ---------------------------------------------------------------------------

#include "global.hpp"
#include "himem.hpp"

#include <cstddef>
#include <cstring>
#include <forward_list>
#include <string>

using CharPoolPtr = HimemUniquePtr<class CharPool>;

class CharPool {
public:
  // ---- Public constants ---------------------------------------------------
  static constexpr std::size_t BLOCK_SIZE = 4096;       ///< PSRAM block size
  static constexpr std::size_t MAX_ALLOC  = BLOCK_SIZE; ///< Hard cap per alloc

private:
  static constexpr char const *TAG = "CharPool";

  // ---- Size-class table ---------------------------------------------------
  // Classes: 2^3=8, 2^4=16, …, 2^12=4096  →  10 classes, index 0..9
  static constexpr std::size_t MIN_LOG   = 3;                     // log2(8)
  static constexpr std::size_t MAX_LOG   = 12;                    // log2(4096)
  static constexpr std::size_t NUM_CLASS = MAX_LOG - MIN_LOG + 1; // 10

  // Return the size-class index for a request of n bytes.
  static constexpr std::size_t classIdx(std::size_t n) noexcept {
    std::size_t sz  = 1u << MIN_LOG; // 8
    std::size_t idx = 0;
    while (sz < n && idx < NUM_CLASS - 1) {
      sz <<= 1;
      ++idx;
    }
    return idx; // 0 = 8B … 9 = 4096B
  }

  // Actual slot size for class index idx.
  static constexpr std::size_t slotBytes(std::size_t idx) noexcept { return 1u << (MIN_LOG + idx); }

  // ---- Free lists (one per size class) ------------------------------------
  struct FreeSlot {
    FreeSlot *next;
  };
  FreeSlot *freeLists_[NUM_CLASS]{};

  // ---- Backing blocks (bump pointer) --------------------------------------
  using PoolPtr = HimemUniquePtr<char[]>;
  std::forward_list<PoolPtr> blockList_;
  char *bump_{nullptr};
  std::size_t bumpRemain_{0};

  // ---- Statistics ---------------------------------------------------------
  std::size_t totalAllocated_{0};
  std::size_t totalFreed_{0};

  // ---- Private helpers ----------------------------------------------------
  bool newBlock() noexcept {
    PoolPtr blk = makeUniqueHimem<char[]>(BLOCK_SIZE);
    if (!blk) return false;
    bump_       = blk.get();
    bumpRemain_ = BLOCK_SIZE;
    blockList_.push_front(std::move(blk));
    return true;
  }

  CharPool() = default;

public:
  ~CharPool() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make() { return makeUniqueHimem<CharPool>(); }

  // ---- Statistics ---------------------------------------------------------
  std::size_t getTotalAllocated() const noexcept { return totalAllocated_; }
  std::size_t getTotalFreed() const noexcept { return totalFreed_; }

  // Legacy accessor (kept for existing callers).
  std::uint32_t get_total_allocated() const noexcept {
    return static_cast<std::uint32_t>(totalAllocated_);
  }

  // ---- Core API -----------------------------------------------------------

  /// Allocate at least @p n bytes. The actual slot may be larger (next
  /// power-of-2 >= 8). Returns nullptr if n > MAX_ALLOC or OOM.
  char *allocate(std::size_t n) noexcept {
    if (n == 0) n = 1;
    if (n > MAX_ALLOC) {
      LOG_E("CharPool: requested %zu bytes exceeds MAX_ALLOC (%zu)", n, MAX_ALLOC);
      return nullptr;
    }

    const std::size_t idx  = classIdx(n);
    const std::size_t slot = slotBytes(idx);

    // Reuse a freed slot when available.
    if (freeLists_[idx]) {
      FreeSlot *s     = freeLists_[idx];
      freeLists_[idx] = s->next;
      totalAllocated_ += slot;
      return reinterpret_cast<char *>(s);
    }

    // Carve from the current block; allocate a new one if needed.
    if (bumpRemain_ < slot) {
      if (!newBlock()) return nullptr;
    }

    char *ptr = bump_;
    bump_ += slot;
    bumpRemain_ -= slot;
    totalAllocated_ += slot;
    return ptr;
  }

  /// Return a slot previously obtained from allocate(@p n).
  /// @p n must equal the value passed to allocate().
  void deallocate(char *ptr, std::size_t n) noexcept {
    if (!ptr || n == 0) return;
    if (n > MAX_ALLOC) return;
    const std::size_t idx  = classIdx(n);
    const std::size_t slot = slotBytes(idx);
    auto *s                = reinterpret_cast<FreeSlot *>(ptr);
    s->next                = freeLists_[idx];
    freeLists_[idx]        = s;
    totalFreed_ += slot;
  }

  /// Copy @p str into the pool and return a pointer to the null-terminated
  /// copy. The returned pointer remains valid until the pool is destroyed.
  char *set(const std::string &str) noexcept {
    const std::size_t len = str.length() + 1;
    char *p               = allocate(len);
    if (p) std::memcpy(p, str.c_str(), len);
    return p;
  }
};

// ---------------------------------------------------------------------------
// CharPoolAllocator<T>
//
// Stateful STL allocator that routes all allocations through a CharPool.
// Suitable for std::basic_string (see PoolString below) and other containers
// whose element lifetime matches the pool's lifetime.
//
// Propagation policy:
//   copy-assignment → does NOT propagate (new container gets its own alloc)
//   move-assignment → propagates (keeps the same pool reference)
//   swap            → propagates
// ---------------------------------------------------------------------------
template <typename T>
class CharPoolAllocator {
public:
  using value_type                             = T;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap            = std::true_type;

  CharPool *pool{nullptr};

  explicit CharPoolAllocator(CharPool *p) noexcept : pool(p) {}

  // Rebind constructor required by std::allocator_traits.
  template <typename U>
  explicit CharPoolAllocator(const CharPoolAllocator<U> &o) noexcept : pool(o.pool) {}

  [[nodiscard]] T *allocate(std::size_t n) {
    void *p = pool->allocate(n * sizeof(T));
    if (!p) return nullptr;
    return static_cast<T *>(p);
  }

  void deallocate(T *ptr, std::size_t n) noexcept {
    pool->deallocate(reinterpret_cast<char *>(ptr), n * sizeof(T));
  }

  template <typename U>
  bool operator==(const CharPoolAllocator<U> &o) const noexcept {
    return pool == o.pool;
  }
  template <typename U>
  bool operator!=(const CharPoolAllocator<U> &o) const noexcept {
    return pool != o.pool;
  }
};

// ---------------------------------------------------------------------------
// PoolString
//
// A std::string whose character buffer is drawn from a CharPool.
// Maximum length: CharPool::MAX_ALLOC - 1 = 4095 characters.
//
// Must be constructed with a CharPoolAllocator, e.g.:
//   PoolString s(CharPoolAllocator<char>(myPool.get()));
//   s = "hello";  // buffer allocated from pool, freed back on destruction
// ---------------------------------------------------------------------------
using PoolString = std::basic_string<char, std::char_traits<char>, CharPoolAllocator<char>>;

// ---------------------------------------------------------------------------
// PoolContext<Tag>
//
// Binds a compile-time Tag type to a single shared CharPool instance.
// Call init() before any StaticPoolAllocator<T,Tag> allocation takes place.
// Call reset() (and then destroy the pool) when all strings using this
// context are destroyed.
//
// Example:
//   struct DomTag {};
//   auto domPool = CharPool::Make();
//   PoolContext<DomTag>::init(domPool.get());
//   ...use DomString freely...
//   PoolContext<DomTag>::reset();
//   domPool.reset(); // single bulk deallocation
// ---------------------------------------------------------------------------
template <typename Tag>
struct PoolContext {
  static CharPool *pool;

  /// Register a CharPool. Must be called before any string of this Tag allocates.
  static void init(CharPool *p) noexcept { pool = p; }

  /// Unregister the pool. After this call, deallocate() in any surviving
  /// string of this Tag is silently skipped (safe no-op).
  static void reset() noexcept { pool = nullptr; }
};

template <typename Tag>
CharPool *PoolContext<Tag>::pool = nullptr;

// ---------------------------------------------------------------------------
// StaticPoolAllocator<T, Tag>
//
// Stateless STL allocator that routes all allocations/deallocations through
// PoolContext<Tag>::pool.  Being stateless (is_always_equal = true) means:
//   - std::basic_string stores no extra bytes for the allocator.
//   - No allocator argument is needed at string construction.
//   - Copy/move/swap of strings automatically share the same pool.
//
// Template parameters:
//   T    — value type (typically char)
//   Tag  — an empty struct that identifies the pool family
// ---------------------------------------------------------------------------
template <typename T, typename Tag>
class StaticPoolAllocator {
public:
  using value_type = T;

  // Stateless — all instances backed by the same static pool.
  using is_always_equal                        = std::true_type;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap            = std::true_type;

  constexpr StaticPoolAllocator() noexcept = default;

  // Rebind constructor: StaticPoolAllocator<U,Tag> → StaticPoolAllocator<T,Tag>.
  template <typename U>
  constexpr StaticPoolAllocator(const StaticPoolAllocator<U, Tag> &) noexcept {}

  [[nodiscard]] T *allocate(std::size_t n) {
    CharPool *p = PoolContext<Tag>::pool;
    if (!p) return nullptr;
    void *mem = p->allocate(n * sizeof(T));
    if (!mem) return nullptr;
    return static_cast<T *>(mem);
  }

  void deallocate(T *ptr, std::size_t n) noexcept {
    if (CharPool *p = PoolContext<Tag>::pool)
      p->deallocate(reinterpret_cast<char *>(ptr), n * sizeof(T));
    // If pool was already reset(), the deallocation is a safe no-op.
  }

  // All instances with the same Tag are equal (they share the same pool).
  template <typename U>
  bool operator==(const StaticPoolAllocator<U, Tag> &) const noexcept {
    return true;
  }
  template <typename U>
  bool operator!=(const StaticPoolAllocator<U, Tag> &) const noexcept {
    return false;
  }
};