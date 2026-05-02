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
// HimemPool<T> satisfies the C++ named requirement Allocator and can be
// used as a stateful allocator for STL containers (e.g. SimpleList<T, HimemPool<T>>).
// When used this way the container owns the pool by value; on move the pool
// moves with the container (propagate_on_container_move_assignment = true);
// on copy a fresh empty pool is created (propagate_on_container_copy_assignment = false).
//
// For direct (non-container) ownership there are two APIs:
//
//   create(args...) -> Ptr
//     Returns a managing std::unique_ptr (Ptr). ~T() and slot recycling happen
//     automatically when the Ptr goes out of scope or is reset().
//
//   newElement(args...) -> T*  /  deleteElement(T*)
//     MemoryPool-compatible raw-pointer API.  Callers must call deleteElement()
//     for every pointer returned by newElement().  Use this when replacing an
//     existing MemoryPool<T> call site that uses those method names.
//
// Example — create() (preferred for new code)
// -------------------------------------------
//   auto pool = HimemPool<MyClass>::Make(32);   // 32 objects per block
//   auto obj  = pool->create(ctorArg1, ctorArg2);
//   // ... use *obj ...
//   obj.reset(); // or let it go out of scope — slot recycled automatically
//
// Example — newElement/deleteElement (drop-in for MemoryPool call sites)
// -----------------------------------------------------------------------
//   auto pool = HimemPool<MyClass>::Make(32);
//   MyClass *obj = pool->newElement(ctorArg1, ctorArg2);
//   // ... use obj ...
//   pool->deleteElement(obj); // calls ~MyClass() and recycles slot
//
// Example — STL container allocator
// ----------------------------------
//   SimpleList<MyClass, HimemPool<MyClass>> list;
//   list.pushBack(value);     // node allocated from embedded pool
//   list.clear();             // nodes freed, slots recycled
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
    deallocate(ptr, 1);
  }

public:
  // ---- STL allocator interface types --------------------------------------
  using value_type = T;

  template <typename U>
  struct rebind {
    using other = HimemPool<U>;
  };

  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap            = std::true_type;

  // ---- Constructors --------------------------------------------------------
  /// Creates a pool with the requested objects-per-block capacity.
  explicit HimemPool(std::size_t blockSize = 20) noexcept
      : blockSize_(blockSize < 1u ? 1u : blockSize) {}

  /// Rebind constructor — creates a fresh pool of U with the same block size.
  /// Required by std::allocator_traits when the allocator is rebound to a
  /// different element type (e.g. from HimemPool<T> to HimemPool<Node<T>>).
  template <typename U>
  explicit HimemPool(const HimemPool<U> &other) noexcept : blockSize_(other.blockSize()) {}

  /// Move constructor — transfers all backing blocks to the new pool.
  /// Outstanding Ptr instances must NOT exist at the time of the move because
  /// their Deleter back-pointer would reference the (now empty) moved-from pool.
  HimemPool(HimemPool &&other) noexcept
      : blockSize_(other.blockSize_), blockList_(std::exchange(other.blockList_, nullptr)),
        currentSlot_(std::exchange(other.currentSlot_, nullptr)),
        lastSlot_(std::exchange(other.lastSlot_, nullptr)),
        freeList_(std::exchange(other.freeList_, nullptr)),
        liveCount_(std::exchange(other.liveCount_, 0)),
        blockCount_(std::exchange(other.blockCount_, 0)) {}

  /// Move assignment — frees current blocks, then steals from other.
  HimemPool &operator=(HimemPool &&other) noexcept {
    if (this != &other) {
      this->~HimemPool();
      ::new (this) HimemPool(std::move(other));
    }
    return *this;
  }

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

  /// Smart-pointer type returned by create().
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

  // ---- STL raw allocate/deallocate (no construction or destruction) --------
  /// Acquires a raw slot.  n must be 1; hint is ignored (mirrors MemoryPool).
  /// Called by std::allocator_traits on behalf of STL containers.
  [[nodiscard]] auto allocate(std::size_t n = 1, const T * /*hint*/ = nullptr) -> T * {
    (void)n; // only single-object allocation is supported, like MemoryPool
    Slot_ *slot = nullptr;

    if (freeList_) {
      slot      = freeList_;
      freeList_ = freeList_->next;
    } else {
      if (currentSlot_ >= lastSlot_) {
        if (!allocateBlock()) return nullptr;
      }
      slot = currentSlot_++;
    }

    ++liveCount_;
    return reinterpret_cast<T *>(slot);
  }

  /// Returns a raw slot to the free list.  Does NOT call the destructor.
  /// Called by std::allocator_traits on behalf of STL containers.
  auto deallocate(T *ptr, std::size_t /*n*/ = 1) noexcept -> void {
    Slot_ *slot = reinterpret_cast<Slot_ *>(ptr);
    slot->next  = freeList_;
    freeList_   = slot;
    --liveCount_;
  }

  // ---- Owning allocation (constructs T, returns managing Ptr) --------------
  /// Constructs a T in the pool with the supplied arguments and returns a
  /// managing Ptr.  Recycles a freed slot when available; otherwise carves
  /// out a fresh slot from the current block, allocating a new block if
  /// necessary.
  ///
  /// @returns A non-null Ptr on success, or a null Ptr on OOM.
  template <typename... Args>
  [[nodiscard]] auto create(Args &&...args) -> Ptr {
    T *ptr = allocate(1, nullptr);
    if (!ptr) return Ptr{nullptr, Deleter{this}};
    ::new (ptr) T(std::forward<Args>(args)...);
    return Ptr{ptr, Deleter{this}};
  }

  // ---- STL allocator equality ---------------------------------------------
  /// Two HimemPool<T> instances are equal only when they are the same object,
  /// since memory allocated by one cannot be freed by another.
  [[nodiscard]] bool operator==(const HimemPool &other) const noexcept { return this == &other; }
  template <typename U>
  [[nodiscard]] bool operator==(const HimemPool<U> &) const noexcept {
    return false;
  }

  /// Raw-pointer release helper for legacy/manual call sites.
  /// Prefer Ptr ownership (create/~Ptr) when possible.
  auto release(T *ptr) noexcept -> void {
    if (ptr) deallocate_(ptr);
  }

  // ---- MemoryPool-compatible raw-pointer API ------------------------------
  // These mirror the MemoryPool<T>::newElement / deleteElement interface so
  // that HimemPool<T> is a drop-in replacement at every call site that uses
  // those names.  Raw pointers returned by newElement() are NOT automatically
  // managed — callers must eventually call deleteElement() on each one.

  /// Constructs a T in the pool and returns a raw owning pointer.
  /// Returns nullptr on OOM.
  template <typename... Args>
  [[nodiscard]] auto newElement(Args &&...args) -> T * {
    T *ptr = allocate(1, nullptr);
    if (ptr) ::new (ptr) T(std::forward<Args>(args)...);
    return ptr;
  }

  /// Destroys the T object and recycles its slot.  Safe to call with nullptr.
  auto deleteElement(T *ptr) noexcept -> void {
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
  /// Copy constructor — creates a fresh EMPTY pool with the same block size.
  /// This is intentionally NOT a deep copy.  The STL allocator contract
  /// requires copy-constructibility for select_on_container_copy_construction;
  /// the copy-constructed allocator starts with no backing blocks.
  HimemPool(const HimemPool &other) noexcept : blockSize_(other.blockSize_) {}

  HimemPool &operator=(const HimemPool &) = delete;

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
