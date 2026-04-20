// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

// ---------------------------------------------------------------------------
// SimpleList<T, Alloc> — A singly-linked list with O(1) pushBack via a tail
// pointer, analogous to std::forward_list<T> but with append support.
//
// Design goals:
//  - Generic template; Alloc defaults to std::allocator<T>.
//  - Swap Alloc for PsramAllocator<T> to get HimemSimpleList<T> whose nodes
//    live in PSRAM (SPIRAM on ESP32 targets, normal heap on Linux).
//  - Head and tail raw pointers for O(1) prepend and append.
//  - Standard-compliant forward iterator (std::forward_iterator_tag).
//  - Full value semantics: copy constructor, copy assignment,
//    move constructor, move assignment.
//  - merge() splices another list onto this one's tail in O(1).
//  - No external dependencies (no MemoryPool, no logging).
// ---------------------------------------------------------------------------

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <initializer_list>

template <typename T, typename Alloc = std::allocator<T>>
class SimpleList {
public:
  // -------------------------------------------------------------------------
  // Node
  // -------------------------------------------------------------------------
  struct Node {
    T     value;
    Node *next{nullptr};

    template <typename... Args>
    explicit Node(Args&&... args)
      : value(std::forward<Args>(args)...), next(nullptr) {}
  };

  // -------------------------------------------------------------------------
  // Allocator machinery
  // -------------------------------------------------------------------------
private:
  using NodeAlloc  = typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
  using NodeTraits = std::allocator_traits<NodeAlloc>;

  [[no_unique_address]] NodeAlloc alloc_;

  template <typename... Args>
  Node *newNode(Args&&... args) {
    Node *p = NodeTraits::allocate(alloc_, 1);
    try {
      NodeTraits::construct(alloc_, p, std::forward<Args>(args)...);
    } catch (...) {
      NodeTraits::deallocate(alloc_, p, 1);
      throw;
    }
    return p;
  }

  void freeNode(Node *p) noexcept {
    NodeTraits::destroy(alloc_, p);
    NodeTraits::deallocate(alloc_, p, 1);
  }

public:
  // -------------------------------------------------------------------------
  // Forward iterator
  // -------------------------------------------------------------------------
  class Iterator {
    friend class SimpleList;
    friend class ConstIterator;
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T *;
    using reference         = T &;

    explicit Iterator(Node *n = nullptr) : current_(n) {}

    reference  operator*()  const { return  current_->value; }
    pointer    operator->() const { return &current_->value; }

    Iterator& operator++()    { current_ = current_->next; return *this; }
    Iterator  operator++(int) { Iterator tmp(*this); ++(*this); return tmp; }

    bool operator==(const Iterator &o) const { return current_ == o.current_; }
    bool operator!=(const Iterator &o) const { return current_ != o.current_; }

  private:
    Node *current_;
  };

  // Const variant
  class ConstIterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const T *;
    using reference         = const T &;

    explicit ConstIterator(const Node *n = nullptr) : current_(n) {}
    // Conversion from mutable iterator — accesses Iterator::current_ via friend.
    /* implicit */ ConstIterator(const Iterator &it) noexcept // NOLINT
      : current_(it.current_) {}

    reference  operator*()  const { return  current_->value; }
    pointer    operator->() const { return &current_->value; }

    ConstIterator& operator++()    { current_ = current_->next; return *this; }
    ConstIterator  operator++(int) { ConstIterator tmp(*this); ++(*this); return tmp; }

    bool operator==(const ConstIterator &o) const { return current_ == o.current_; }
    bool operator!=(const ConstIterator &o) const { return current_ != o.current_; }

  private:
    const Node *current_;
  };

  // -------------------------------------------------------------------------
  // Construction / destruction
  // -------------------------------------------------------------------------
  SimpleList() = default;

  explicit SimpleList(const Alloc &a) : alloc_(a) {}

  explicit SimpleList(std::initializer_list<T> init, const Alloc &a = Alloc{})
    : alloc_(a) {
    for (const T &v : init) pushBack(v);
  }

  // Copy constructor — propagate allocator according to POCCA rule.
  SimpleList(const SimpleList &other)
    : alloc_(NodeTraits::select_on_container_copy_construction(other.alloc_)) {
    for (const T &v : other) pushBack(v);
  }

  // Move constructor — steal resources; move allocator per POCMA.
  SimpleList(SimpleList &&other) noexcept
    : alloc_(std::move(other.alloc_))
    , head_(other.head_), tail_(other.tail_), count_(other.count_) {
    other.head_  = nullptr;
    other.tail_  = nullptr;
    other.count_ = 0;
  }

  ~SimpleList() { clear(); }

  // -------------------------------------------------------------------------
  // Assignment
  // -------------------------------------------------------------------------
  SimpleList& operator=(const SimpleList &other) {
    if (this != &other) {
      clear();
      if constexpr (NodeTraits::propagate_on_container_copy_assignment::value)
        alloc_ = other.alloc_;
      for (const T &v : other) pushBack(v);
    }
    return *this;
  }

  SimpleList& operator=(SimpleList &&other) noexcept {
    if (this != &other) {
      clear();
      if constexpr (NodeTraits::propagate_on_container_move_assignment::value)
        alloc_ = std::move(other.alloc_);
      head_  = other.head_;
      tail_  = other.tail_;
      count_ = other.count_;
      other.head_  = nullptr;
      other.tail_  = nullptr;
      other.count_ = 0;
    }
    return *this;
  }

  // -------------------------------------------------------------------------
  // Allocator access
  // -------------------------------------------------------------------------
  Alloc getAllocator() const noexcept {
    return static_cast<Alloc>(alloc_);
  }

  // -------------------------------------------------------------------------
  // Element access
  // -------------------------------------------------------------------------
  T&       front()       { return head_->value; }
  const T& front() const { return head_->value; }

  T&       back()        { return tail_->value; }
  const T& back()  const { return tail_->value; }

  /// Returns the tail Node pointer (mirrors DisplayList::last()).
  Node *last() const { return tail_; }

  // -------------------------------------------------------------------------
  // Modifiers
  // -------------------------------------------------------------------------

  /// Insert a copy at the front — O(1).
  void pushFront(const T &value) {
    Node *n = newNode(value);
    n->next = head_;
    head_   = n;
    if (tail_ == nullptr) tail_ = n;
    ++count_;
  }

  void pushFront(T &&value) {
    Node *n = newNode(std::move(value));
    n->next = head_;
    head_   = n;
    if (tail_ == nullptr) tail_ = n;
    ++count_;
  }

  template <typename... Args>
  T& emplaceFront(Args&&... args) {
    Node *n = newNode(std::forward<Args>(args)...);
    n->next = head_;
    head_   = n;
    if (tail_ == nullptr) tail_ = n;
    ++count_;
    return n->value;
  }

  /// Append a copy at the back — O(1).
  void pushBack(const T &value) {
    Node *n = newNode(value);
    if (tail_ == nullptr) {
      head_ = tail_ = n;
    } else {
      tail_->next = n;
      tail_       = n;
    }
    ++count_;
  }

  void pushBack(T &&value) {
    Node *n = newNode(std::move(value));
    if (tail_ == nullptr) {
      head_ = tail_ = n;
    } else {
      tail_->next = n;
      tail_       = n;
    }
    ++count_;
  }

  template <typename... Args>
  T& emplaceBack(Args&&... args) {
    Node *n = newNode(std::forward<Args>(args)...);
    if (tail_ == nullptr) {
      head_ = tail_ = n;
    } else {
      tail_->next = n;
      tail_       = n;
    }
    ++count_;
    return n->value;
  }

  /// Remove the front element — O(1). UB if empty.
  void popFront() {
    Node *old = head_;
    head_ = head_->next;
    if (head_ == nullptr) tail_ = nullptr;
    freeNode(old);
    --count_;
  }

  /// Remove the last element — O(n). Does nothing if empty.
  void removeLast() {
    if (head_ == nullptr) return;
    if (head_ == tail_) {
      freeNode(head_);
      head_ = tail_ = nullptr;
      count_ = 0;
      return;
    }
    Node *prev = head_;
    while (prev->next != tail_) prev = prev->next;
    freeNode(tail_);
    tail_       = prev;
    tail_->next = nullptr;
    --count_;
  }

  /// Splice all nodes from \p other onto the end of this list — O(1).
  /// \p other is left empty after the call.
  /// Precondition: both lists must share the same allocator.
  void merge(SimpleList &other) {
    if (other.empty()) return;
    if (empty()) {
      head_  = other.head_;
      tail_  = other.tail_;
      count_ = other.count_;
    } else {
      tail_->next = other.head_;
      tail_       = other.tail_;
      count_     += other.count_;
    }
    other.head_  = nullptr;
    other.tail_  = nullptr;
    other.count_ = 0;
  }

  /// Destroy all nodes.
  void clear() {
    Node *cur = head_;
    while (cur != nullptr) {
      Node *next = cur->next;
      freeNode(cur);
      cur = next;
    }
    head_  = nullptr;
    tail_  = nullptr;
    count_ = 0;
  }

  // -------------------------------------------------------------------------
  // Capacity
  // -------------------------------------------------------------------------
  bool   empty() const { return head_ == nullptr; }
  size_t size()  const { return count_; }

  // -------------------------------------------------------------------------
  // Iterators
  // -------------------------------------------------------------------------
  Iterator      begin()        { return Iterator(head_);        }
  Iterator      end()          { return Iterator(nullptr);      }
  ConstIterator begin()  const { return ConstIterator(head_);   }
  ConstIterator end()    const { return ConstIterator(nullptr);  }
  ConstIterator cbegin() const { return ConstIterator(head_);   }
  ConstIterator cend()   const { return ConstIterator(nullptr);  }

private:
  Node   *head_{nullptr};
  Node   *tail_{nullptr};
  size_t  count_{0};
};
