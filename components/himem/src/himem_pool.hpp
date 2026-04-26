// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

// ---------------------------------------------------------------------------
// HimemPool<T>
//
// A PSRAM-backed per-type memory pool.  Memory is acquired from PSRAM in
// blocks of `blockSize` objects at a time (default: 20).  Individual objects
// are handed to callers one at a time; freed slots are recycled for subsequent
// allocations.  T's constructor and destructor are called at the appropriate
// moments.
//
// The pool object can be constructed directly (stack/heap) or allocated in
// PSRAM using HimemPool<T>::Make(blockSize).
//
// Each individual object allocation is returned as a HimemPool<T>::Ptr,
// a std::unique_ptr<T, HimemPool<T>::Deleter> that, when destroyed (or
// reset), calls ~T() and returns the slot to the pool's free list.
//
// Example
// -------
//   auto pool = HimemPool<MyClass>::Make(32);   // 32 objects per block
//   auto obj  = pool->allocate(ctorArg1, ctorArg2);
//   // ... use *obj ...
//   obj.reset(); // or let it go out of scope — slot recycled automatically
//
// IMPORTANT: All Ptr values MUST be destroyed before the pool itself.
// The pool destructor does NOT call destructors on any still-live objects.
// ---------------------------------------------------------------------------

#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "himem.hpp"

template <typename T>
class HimemPool {
private:
  // ---- Slot union ----------------------------------------------------------
  // Each slot stores either a live T (when in use) or a pointer to the next
  // free slot (when recycled).  Explicit user-provided constructor/destructor
  // prevent compiler errors for T types with non-trivial special members;
  // the object lifecycle is managed manually via placement new / explicit dtor.
  union Slot_ {
    T element;
    Slot_ *next;
    Slot_() noexcept {}  // do not construct T
    ~Slot_() noexcept {} // do not destroy T
  };

  // ---- Block header --------------------------------------------------------
  // Aligned to std::max_align_t so that the Slot_ array that follows in the
  // same PSRAM allocation is suitably aligned for any standard type T.
  struct alignas(std::max_align_t) Block_ {
    Block_ *next{nullptr}; ///< previous block in pool's block list
  };

  // ---- Private data --------------------------------------------------------
  std::size_t blockSize_;       ///< number of Slot_ per block
  Block_ *blockList_{nullptr};  ///< linked list of all blocks
  Slot_ *currentSlot_{nullptr}; ///< next fresh slot in current block
  Slot_ *lastSlot_{nullptr};    ///< one-past-end of current block's slot array
  Slot_ *freeList_{nullptr};    ///< singly-linked list of recycled slots
  std::size_t liveCount_{0};    ///< number of objects currently alive
  std::size_t blockCount_{0};   ///< number of backing blocks allocated

  // ---- Internal helpers ----------------------------------------------------

  /// Returns a pointer to the Slot_ array that follows a Block_ header.
  /// Safe because sizeof(Block_) is a multiple of alignof(std::max_align_t),
  /// which is >= alignof(Slot_) for any standard type T.
  static Slot_ *slotsOf(Block_ *block) noexcept { return reinterpret_cast<Slot_ *>(block + 1); }

  /// Allocates a new backing block from PSRAM.
  /// Returns true on success, false on OOM.
  bool allocateBlock() noexcept {
    const std::size_t blockBytes = sizeof(Block_) + blockSize_ * sizeof(Slot_);

    PsramAllocator<char> psramAlloc;
    char *mem = psramAlloc.allocate(blockBytes);
    if (!mem) return false;

    Block_ *block = ::new (mem) Block_();
    block->next   = blockList_;
    blockList_    = block;
    ++blockCount_;

    currentSlot_ = slotsOf(block);
    lastSlot_    = currentSlot_ + blockSize_;
    return true;
  }

  /// Called by Deleter: destroys the T object and recycles its slot.
  void deallocate_(T *ptr) noexcept {
    ptr->~T();
    Slot_ *slot = reinterpret_cast<Slot_ *>(ptr);
    slot->next  = freeList_;
    freeList_   = slot;
    --liveCount_;
  }

public:
  // ---- Constructors --------------------------------------------------------
  /// Creates a pool with the requested objects-per-block capacity.
  explicit HimemPool(std::size_t blockSize = 20) noexcept
      : blockSize_(blockSize < 1u ? 1u : blockSize) {}

  // ---- Custom deleter -------------------------------------------------------
  // Held by every Ptr returned from allocate().  On destruction it calls
  // ~T() on the managed object and recycles its slot back to the pool.
  //
  // The pool pointer is non-owning; the pool must outlive all Ptr instances.
  struct Deleter {
    HimemPool *pool{nullptr};

    constexpr Deleter() noexcept = default;
    explicit Deleter(HimemPool *p) noexcept : pool(p) {}

    void operator()(T *ptr) const noexcept {
      if (ptr && pool) pool->deallocate_(ptr);
    }
  };

  /// Smart-pointer type returned by allocate().
  using Ptr = std::unique_ptr<T, Deleter>;

  // ---- Factory -------------------------------------------------------------
  /// Creates a new HimemPool<T> in PSRAM and returns a unique owning pointer.
  /// @param blockSize  Number of T objects per backing block (default: 20).
  /// @returns          A HimemUniquePtr<HimemPool<T>>, or nullptr on OOM.
  [[nodiscard]] static auto Make(std::size_t blockSize = 20) -> HimemUniquePtr<HimemPool> {
    PsramAllocator<HimemPool> psramAlloc;
    HimemPool *mem = psramAlloc.allocate(1);
    if (!mem) return nullptr;

    HimemPool *pool = ::new (mem) HimemPool(blockSize);
    return HimemUniquePtr<HimemPool>{pool};
  }

  // ---- Object allocation ---------------------------------------------------
  /// Constructs a T in the pool with the supplied arguments and returns a
  /// managing Ptr.  Recycles a freed slot when available; otherwise carves
  /// out a fresh slot from the current block, allocating a new block if
  /// necessary.
  ///
  /// @returns A non-null Ptr on success, or a null Ptr on OOM.
  template <typename... Args>
  [[nodiscard]] auto allocate(Args &&...args) -> Ptr {
    Slot_ *slot = nullptr;

    if (freeList_) {
      // Reuse a previously freed slot.
      slot      = freeList_;
      freeList_ = freeList_->next;
    } else {
      // Carve out the next fresh slot, growing the pool if needed.
      if (currentSlot_ >= lastSlot_) {
        if (!allocateBlock()) return Ptr{nullptr, Deleter{this}};
      }
      slot = currentSlot_++;
    }

    T *ptr = ::new (&slot->element) T(std::forward<Args>(args)...);
    ++liveCount_;
    return Ptr{ptr, Deleter{this}};
  }

  /// Raw-pointer release helper for legacy/manual call sites.
  /// Prefer Ptr ownership when possible.
  auto deallocate(T *ptr) noexcept -> void {
    if (ptr) deallocate_(ptr);
  }

  // ---- Statistics ----------------------------------------------------------
  /// Number of objects currently alive (allocated but not yet freed).
  [[nodiscard]] std::size_t liveCount() const noexcept { return liveCount_; }
  /// Number of backing blocks currently held in PSRAM.
  [[nodiscard]] std::size_t blockCount() const noexcept { return blockCount_; }
  /// Capacity configured for each block (objects per block).
  [[nodiscard]] std::size_t blockSize() const noexcept { return blockSize_; }

  // ---- Lifecycle -----------------------------------------------------------
  HimemPool(const HimemPool &)            = delete;
  HimemPool &operator=(const HimemPool &) = delete;
  HimemPool(HimemPool &&)                 = delete;
  HimemPool &operator=(HimemPool &&)      = delete;

  /// Releases all backing blocks back to PSRAM.
  /// WARNING: Does NOT call destructors on any live objects still referenced
  /// by outstanding Ptr values.  Ensure all Ptr instances are destroyed (or
  /// reset) before the pool is destroyed.
  ~HimemPool() noexcept {
    assert(liveCount_ == 0 && "HimemPool destroyed with live objects: all Ptr instances must be "
                              "released before the pool is destroyed");

    PsramAllocator<char> psramAlloc;
    const std::size_t blockBytes = sizeof(Block_) + blockSize_ * sizeof(Slot_);

    Block_ *curr = blockList_;
    while (curr) {
      Block_ *prev = curr->next;
      psramAlloc.deallocate(reinterpret_cast<char *>(curr), blockBytes);
      curr = prev;
    }
  }
};
