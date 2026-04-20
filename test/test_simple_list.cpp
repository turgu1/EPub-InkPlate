// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// Test suite for SimpleList<T>
//
// Covers:
//  • Default construction, empty() and size()
//  • pushBack (copy and move)
//  • pushFront (copy and move)
//  • emplaceBack / emplaceFront
//  • front() / back() / last()
//  • popFront()
//  • removeLast()
//  • Iteration (range-for, begin/end, cbegin/cend)
//  • clear()
//  • merge()
//  • Initializer-list construction
//  • Copy constructor and copy assignment
//  • Move constructor and move assignment
//  • Works with non-trivial T (std::string)
// ---------------------------------------------------------------------------

#include "simple_list.hpp"

#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Minimal check helpers (same style as the other test suites)
// ---------------------------------------------------------------------------
static int  checks  = 0;
static int  failures = 0;

#define CHECK(cond) \
  do { \
    ++checks; \
    if (!(cond)) { \
      ++failures; \
      std::printf("  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
    } \
  } while (0)

// ============================================================
// Tests
// ============================================================

static void testConstruction() {
  std::printf("  [construction]\n");

  SimpleList<int> a;
  CHECK(a.empty());
  CHECK(a.size() == 0);

  SimpleList<int> b{1, 2, 3};
  CHECK(!b.empty());
  CHECK(b.size() == 3);
  CHECK(b.front() == 1);
  CHECK(b.back()  == 3);
}

static void testPushBack() {
  std::printf("  [pushBack]\n");

  SimpleList<int> l;
  l.pushBack(10);
  CHECK(l.size()  == 1);
  CHECK(l.front() == 10);
  CHECK(l.back()  == 10);

  l.pushBack(20);
  CHECK(l.size()  == 2);
  CHECK(l.front() == 10);
  CHECK(l.back()  == 20);

  int v = 30;
  l.pushBack(v);
  CHECK(l.size()  == 3);
  CHECK(l.back()  == 30);
}

static void testPushFront() {
  std::printf("  [pushFront]\n");

  SimpleList<int> l;
  l.pushFront(1);
  l.pushFront(2);
  l.pushFront(3);
  CHECK(l.size()  == 3);
  CHECK(l.front() == 3);
  CHECK(l.back()  == 1);
}

static void testEmplace() {
  std::printf("  [emplaceBack / emplaceFront]\n");

  SimpleList<std::string> l;
  l.emplaceBack("hello");
  l.emplaceBack("world");
  CHECK(l.size()  == 2);
  CHECK(l.front() == "hello");
  CHECK(l.back()  == "world");

  l.emplaceFront("first");
  CHECK(l.front() == "first");
  CHECK(l.size()  == 3);
}

static void testAccessors() {
  std::printf("  [front / back / last]\n");

  SimpleList<int> l{5, 10, 15};
  CHECK(l.front() == 5);
  CHECK(l.back()  == 15);

  auto *node = l.last();
  CHECK(node != nullptr);
  CHECK(node->value == 15);
  CHECK(node->next  == nullptr);
}

static void testPopFront() {
  std::printf("  [popFront]\n");

  SimpleList<int> l{1, 2, 3};
  l.popFront();
  CHECK(l.size()  == 2);
  CHECK(l.front() == 2);

  l.popFront();
  l.popFront();
  CHECK(l.empty());
  CHECK(l.size() == 0);
}

static void testRemoveLast() {
  std::printf("  [removeLast]\n");

  SimpleList<int> l{1, 2, 3};
  l.removeLast();
  CHECK(l.size()  == 2);
  CHECK(l.back()  == 2);
  CHECK(l.front() == 1);

  l.removeLast();
  CHECK(l.size()  == 1);
  CHECK(l.front() == 1);
  CHECK(l.back()  == 1);

  l.removeLast();
  CHECK(l.empty());

  // removeLast on empty list — must not crash
  l.removeLast();
  CHECK(l.empty());
}

static void testIteration() {
  std::printf("  [iteration]\n");

  SimpleList<int> l{10, 20, 30, 40};

  int sum = 0;
  for (int v : l) sum += v;
  CHECK(sum == 100);

  // count via iterator
  int cnt = 0;
  for (auto it = l.cbegin(); it != l.cend(); ++it) ++cnt;
  CHECK(cnt == 4);

  // post-increment
  auto it = l.begin();
  CHECK(*it == 10);
  it++;
  CHECK(*it == 20);
}

static void testClear() {
  std::printf("  [clear]\n");

  SimpleList<int> l{1, 2, 3, 4, 5};
  l.clear();
  CHECK(l.empty());
  CHECK(l.size() == 0);

  // Clearing an already-empty list must not crash
  l.clear();
  CHECK(l.empty());

  // Re-use after clear
  l.pushBack(99);
  CHECK(l.size()  == 1);
  CHECK(l.front() == 99);
  CHECK(l.back()  == 99);
}

static void testMerge() {
  std::printf("  [merge]\n");

  SimpleList<int> a{1, 2, 3};
  SimpleList<int> b{4, 5, 6};

  a.merge(b);
  CHECK(a.size() == 6);
  CHECK(b.empty());
  CHECK(a.front() == 1);
  CHECK(a.back()  == 6);

  // Verify full sequence
  int expected[] = {1, 2, 3, 4, 5, 6};
  int idx = 0;
  for (int v : a) CHECK(v == expected[idx++]);
  CHECK(idx == 6);

  // Merge empty list into non-empty — source stays empty, dest unchanged
  SimpleList<int> c{7};
  SimpleList<int> d;   // empty
  c.merge(d);
  CHECK(c.size() == 1);
  CHECK(c.back() == 7);

  // Merge non-empty into empty
  SimpleList<int> e;
  SimpleList<int> f{8, 9};
  e.merge(f);
  CHECK(e.size() == 2);
  CHECK(e.front() == 8);
  CHECK(e.back()  == 9);
  CHECK(f.empty());
}

static void testCopySemantics() {
  std::printf("  [copy constructor / assignment]\n");

  SimpleList<int> orig{1, 2, 3};

  SimpleList<int> copy_ctor(orig);
  CHECK(copy_ctor.size()  == 3);
  CHECK(copy_ctor.front() == 1);
  CHECK(copy_ctor.back()  == 3);

  // Modifying copy does not affect original
  copy_ctor.pushBack(4);
  CHECK(orig.size()      == 3);
  CHECK(copy_ctor.size() == 4);

  SimpleList<int> copy_assign;
  copy_assign = orig;
  CHECK(copy_assign.size()  == 3);
  CHECK(copy_assign.front() == 1);

  // Self-assignment
  copy_assign = copy_assign; // NOLINT
  CHECK(copy_assign.size() == 3);
}

static void testMoveSemantics() {
  std::printf("  [move constructor / assignment]\n");

  SimpleList<int> src{10, 20, 30};

  SimpleList<int> moved_ctor(std::move(src));
  CHECK(moved_ctor.size() == 3);
  CHECK(src.empty());
  CHECK(moved_ctor.front() == 10);
  CHECK(moved_ctor.back()  == 30);

  SimpleList<int> moved_assign;
  moved_assign = std::move(moved_ctor);
  CHECK(moved_assign.size() == 3);
  CHECK(moved_ctor.empty());
  CHECK(moved_assign.front() == 10);
}

static void testMoveValues() {
  std::printf("  [pushBack(T&&) / pushFront(T&&)]\n");

  SimpleList<std::string> l;
  std::string s1 = "foo";
  std::string s2 = "bar";
  l.pushBack(std::move(s1));
  l.pushFront(std::move(s2));
  CHECK(l.size()  == 2);
  CHECK(l.front() == "bar");
  CHECK(l.back()  == "foo");
}

static void testSingleElement() {
  std::printf("  [single-element edge cases]\n");

  SimpleList<int> l;
  l.pushBack(42);
  CHECK(l.front() == l.back());
  CHECK(&l.last()->value == &l.front());

  l.popFront();
  CHECK(l.empty());

  l.pushFront(7);
  CHECK(!l.empty());
  CHECK(l.front() == 7);
  CHECK(l.back()  == 7);
}

// ============================================================
// Suite entry point
// ============================================================
auto testSimpleList() -> bool {
  checks   = 0;
  failures = 0;

  testConstruction();
  testPushBack();
  testPushFront();
  testEmplace();
  testAccessors();
  testPopFront();
  testRemoveLast();
  testIteration();
  testClear();
  testMerge();
  testCopySemantics();
  testMoveSemantics();
  testMoveValues();
  testSingleElement();

  std::printf("  SimpleList: %d checks, %d failures\n", checks, failures);
  return failures == 0;
}
