// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// himem test suite
//
// Full implementation lives here (Linux test build).  The ESP32 build uses
// the copy in components/himem/src/himem_test.cpp which is kept in sync.
// ---------------------------------------------------------------------------

#include "himem.hpp"

#include <cstdio>
#include <limits>

#define HT_LOG(fmt, ...) std::printf("[himem_test] " fmt "\n", ##__VA_ARGS__)
#define IS_PSRAM(ptr) true // no SPIRAM on host — skip address checks

// ===========================================================================
// Internal helpers
// ===========================================================================

namespace {

static int sPass = 0;
static int sFail = 0;

#define HT_CHECK(cond, msg)                                                                        \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      HT_LOG("FAIL [%s:%d] " msg, __FILE__, __LINE__);                                             \
      ++sFail;                                                                                     \
    } else {                                                                                       \
      HT_LOG("PASS " msg);                                                                         \
      ++sPass;                                                                                     \
    }                                                                                              \
  } while (0)

// ---------------------------------------------------------------------------
// Tracked — counts live instances to verify constructor/destructor pairing.
// ---------------------------------------------------------------------------
struct Tracked {
  static int live;
  int value;
  explicit Tracked(int v) noexcept : value(v) { ++live; }
  Tracked(const Tracked &o) noexcept : value(o.value) { ++live; }
  ~Tracked() noexcept { --live; }
};
int Tracked::live = 0;

// ===========================================================================
// 1. PsramAllocator<T>
// ===========================================================================
static auto testAllocator() -> void {
  HT_LOG("--- PsramAllocator ---");

  PsramAllocator<int> alloc;

  int *ptr = alloc.allocate(8);
  HT_CHECK(ptr != nullptr, "allocate returns non-null");
  HT_CHECK(IS_PSRAM(ptr), "allocate returns PSRAM pointer");

  ptr[0] = 0x1234;
  ptr[7] = 0x5678;
  HT_CHECK(ptr[0] == 0x1234 && ptr[7] == 0x5678, "allocated memory is writable");

  alloc.deallocate(ptr, 8); // must not crash
  HT_CHECK(true, "deallocate does not crash");

  // Stateless: instances of different value_type must compare equal.
  PsramAllocator<float> alloc2;
  HT_CHECK((alloc == alloc2), "different-typed allocator instances compare equal");
}

// ===========================================================================
// 2. PsramDeleter<T> — scalar
// ===========================================================================
static auto testDeleterScalar() -> void {
  HT_LOG("--- PsramDeleter<T> scalar ---");

  Tracked::live = 0;
  PsramAllocator<Tracked> alloc;
  Tracked *p = alloc.allocate(1);
  ::new (p) Tracked(42);
  HT_CHECK(Tracked::live == 1, "constructor called once");

  PsramDeleter<Tracked> del;
  del(p); // calls ~Tracked() then frees
  HT_CHECK(Tracked::live == 0, "destructor called by scalar PsramDeleter");
}

// ===========================================================================
// 3. PsramDeleter<T[]> — array
// ===========================================================================
static auto testDeleterArray() -> void {
  HT_LOG("--- PsramDeleter<T[]> array ---");

  constexpr std::size_t N = 5;
  Tracked::live           = 0;

  PsramAllocator<Tracked> alloc;
  Tracked *arr = alloc.allocate(N);
  for (std::size_t i = 0; i < N; ++i) ::new (arr + i) Tracked(static_cast<int>(i));
  HT_CHECK(Tracked::live == static_cast<int>(N), "N constructors called");

  PsramDeleter<Tracked[]> del{N};
  del(arr); // calls N destructors then frees
  HT_CHECK(Tracked::live == 0, "all destructors called by array PsramDeleter");
}

// ===========================================================================
// 4. makeSharedHimem<T> — scalar
// ===========================================================================
static auto testSharedScalar() -> void {
  HT_LOG("--- makeSharedHimem<T> scalar ---");

  Tracked::live = 0;
  auto p        = makeSharedHimem<Tracked>(7);

  HT_CHECK(p != nullptr, "makeSharedHimem returns non-null");
  HT_CHECK(IS_PSRAM(p.get()), "managed object is in PSRAM");
  HT_CHECK(p->value == 7, "object constructed with correct value");
  HT_CHECK(Tracked::live == 1, "one live instance after makeSharedHimem");
  HT_CHECK(p.use_count() == 1, "use_count starts at 1");

  {
    auto p2 = p; // shared copy
    HT_CHECK(p.use_count() == 2, "use_count 2 after copy");
    HT_CHECK(Tracked::live == 1, "still one live object after shared copy");
  }
  HT_CHECK(p.use_count() == 1, "use_count back to 1 after copy leaves scope");
  HT_CHECK(Tracked::live == 1, "object survives copy-owner leaving scope");

  p.reset();
  HT_CHECK(!p, "ptr null after reset");
  HT_CHECK(Tracked::live == 0, "destructor called when last owner resets");
}

// ===========================================================================
// 5. makeSharedHimem<T[]> — unbounded array
// ===========================================================================
static auto testSharedArray() -> void {
  HT_LOG("--- makeSharedHimem<T[]> unbounded array ---");

  constexpr std::size_t N = 10;
  auto arr                = makeSharedHimem<int[]>(N);

  HT_CHECK(arr != nullptr, "makeSharedHimem array returns non-null");
  HT_CHECK(IS_PSRAM(arr.get()), "array storage is in PSRAM");

  for (std::size_t i = 0; i < N; ++i) arr[i] = static_cast<int>(i * i);

  bool ok = true;
  for (std::size_t i = 0; i < N; ++i) ok = ok && (arr[i] == static_cast<int>(i * i));
  HT_CHECK(ok, "array elements written and read back correctly");
}

// ===========================================================================
// 6. makeUniqueHimem<T> — scalar
// ===========================================================================
static auto testUniqueScalar() -> void {
  HT_LOG("--- makeUniqueHimem<T> scalar ---");

  Tracked::live = 0;
  auto u        = makeUniqueHimem<Tracked>(99);

  HT_CHECK(u != nullptr, "makeUniqueHimem returns non-null");
  HT_CHECK(IS_PSRAM(u.get()), "managed object is in PSRAM");
  HT_CHECK(u->value == 99, "object constructed with correct value");
  HT_CHECK(Tracked::live == 1, "one live instance");

  auto u2 = std::move(u);
  HT_CHECK(!u, "source unique_ptr null after move");
  HT_CHECK(u2 != nullptr, "destination valid after move");
  HT_CHECK(Tracked::live == 1, "still one live instance after move");

  u2.reset();
  HT_CHECK(!u2, "ptr null after reset");
  HT_CHECK(Tracked::live == 0, "destructor called on reset");
}

// ===========================================================================
// 7. makeUniqueHimem<T[]> — unbounded array
// ===========================================================================
static auto testUniqueArray() -> void {
  HT_LOG("--- makeUniqueHimem<T[]] unbounded array ---");

  constexpr std::size_t N = 6;
  auto arr                = makeUniqueHimem<int[]>(N);

  HT_CHECK(arr != nullptr, "makeUniqueHimem array returns non-null");
  HT_CHECK(IS_PSRAM(arr.get()), "array storage is in PSRAM");

  // Elements must be value-initialised (zero for int).
  bool zeroed = true;
  for (std::size_t i = 0; i < N; ++i) zeroed = zeroed && (arr[i] == 0);
  HT_CHECK(zeroed, "array elements value-initialised to zero");

  for (std::size_t i = 0; i < N; ++i) arr[i] = static_cast<int>(i + 1);
  bool ok = true;
  for (std::size_t i = 0; i < N; ++i) ok = ok && (arr[i] == static_cast<int>(i + 1));
  HT_CHECK(ok, "array elements written and read back correctly");
}

// ===========================================================================
// 8. HimemString
// ===========================================================================
static auto testHimemString() -> void {
  HT_LOG("--- HimemString ---");

  HimemString s;
  HT_CHECK(s.empty(), "default-constructed HimemString is empty");

  // Use a string >= 32 chars to guarantee the custom allocator is invoked
  // (bypass potential small-string-optimisation in the platform's libstdc++).
  s = "Hello from PSRAM string buffer!!"; // 32 chars
  HT_CHECK(s == "Hello from PSRAM string buffer!!", "assignment and comparison correct");
  HT_CHECK(s.size() == 32, "size() correct after assignment");
  HT_CHECK(IS_PSRAM(s.data()), "character buffer is in PSRAM");

  s += " Extended.";
  HT_CHECK(s.find("Extended") != HimemString::npos, "append and find work correctly");

  HimemString s2{s}; // copy
  HT_CHECK(s2 == s, "copy-constructed string equals original");
  HT_CHECK(IS_PSRAM(s2.data()), "copy's character buffer is in PSRAM");

  s2.clear();
  HT_CHECK(s2.empty(), "clear() empties string");
  HT_CHECK(!s.empty(), "original unaffected by copy-then-clear");
}

// ===========================================================================
// 9. HimemVector<int>
// ===========================================================================
static auto testHimemVector() -> void {
  HT_LOG("--- HimemVector<int> ---");

  HimemVector<int> v;
  HT_CHECK(v.empty(), "default-constructed HimemVector is empty");

  for (int i = 0; i < 8; ++i) v.push_back(i * 3);
  HT_CHECK(v.size() == 8, "size() correct after push_back");
  HT_CHECK(IS_PSRAM(v.data()), "element storage is in PSRAM");

  bool ok = true;
  for (int i = 0; i < 8; ++i) ok = ok && (v[i] == i * 3);
  HT_CHECK(ok, "elements have correct values");

  // Force a reallocation.
  v.reserve(128);
  HT_CHECK(v.capacity() >= 128, "reserve() grows capacity");
  HT_CHECK(IS_PSRAM(v.data()), "element storage still in PSRAM after realloc");

  v.clear();
  HT_CHECK(v.empty(), "clear() empties vector");

  // Verify destructor pairing with a non-trivial element type.
  Tracked::live = 0;
  {
    HimemVector<Tracked> tv;
    tv.emplace_back(1);
    tv.emplace_back(2);
    tv.emplace_back(3);
    HT_CHECK(Tracked::live == 3, "3 Tracked objects live in HimemVector");
    HT_CHECK(IS_PSRAM(tv.data()), "Tracked element storage is in PSRAM");
  }
  HT_CHECK(Tracked::live == 0, "all Tracked destructors called when HimemVector leaves scope");
}

// ===========================================================================
// 10. himemUniqueString
// ===========================================================================
static auto testUniqueString() -> void {
  HT_LOG("--- himemUniqueString ---");

  auto s = makeUniqueHimemString("Hello from PSRAM unique string!!");

  HT_CHECK(s != nullptr, "makeUniqueHimemString returns non-null");
  HT_CHECK(IS_PSRAM(s.get()), "HimemString object itself is in PSRAM");
  HT_CHECK(*s == "Hello from PSRAM unique string!!", "content is correct");
  HT_CHECK(IS_PSRAM(s->data()), "character buffer is in PSRAM");

  // Move: ownership must transfer cleanly.
  auto s2 = std::move(s);
  HT_CHECK(!s, "source null after move");
  HT_CHECK(s2 != nullptr, "destination valid after move");
  HT_CHECK(*s2 == "Hello from PSRAM unique string!!", "content preserved after move");
  HT_CHECK(IS_PSRAM(s2.get()), "moved-to object still in PSRAM");

  // Empty / default construction then mutate.
  auto empty = makeUniqueHimemString();
  HT_CHECK(empty != nullptr, "empty makeUniqueHimemString returns non-null");
  HT_CHECK(empty->empty(), "default-constructed string is empty");
  HT_CHECK(IS_PSRAM(empty.get()), "empty string object is in PSRAM");

  empty->assign("Assigned after default construction!!");
  HT_CHECK(*empty == "Assigned after default construction!!",
           "assign after default-construction works");
}

// ===========================================================================
// 11. himemUniqueVector<T>
// ===========================================================================
static auto testUniqueVector() -> void {
  HT_LOG("--- himemUniqueVector<int> ---");

  // Sized construction: 5 elements, each 7.
  auto v = makeUniqueHimemVector<int>(5u, 7);

  HT_CHECK(v != nullptr, "makeUniqueHimemVector returns non-null");
  HT_CHECK(IS_PSRAM(v.get()), "HimemVector<int> object itself is in PSRAM");
  HT_CHECK(v->size() == 5, "vector has correct initial size");
  HT_CHECK(IS_PSRAM(v->data()), "element storage is in PSRAM");

  bool ok = true;
  for (std::size_t i = 0; i < 5; ++i) ok = ok && ((*v)[i] == 7);
  HT_CHECK(ok, "all elements have correct initial value");

  v->push_back(42);
  HT_CHECK(v->size() == 6 && v->back() == 42, "push_back through unique_ptr works");

  // Move: ownership must transfer cleanly.
  auto v2 = std::move(v);
  HT_CHECK(!v, "source null after move");
  HT_CHECK(v2 != nullptr, "destination valid after move");
  HT_CHECK(v2->size() == 6, "content preserved after move");
  HT_CHECK(IS_PSRAM(v2.get()), "moved-to object still in PSRAM");

  // Empty / default construction.
  auto ev = makeUniqueHimemVector<float>();
  HT_CHECK(ev != nullptr, "empty makeUniqueHimemVector returns non-null");
  HT_CHECK(ev->empty(), "default-constructed vector is empty");
  HT_CHECK(IS_PSRAM(ev.get()), "empty vector object itself is in PSRAM");

  // Verify destructor pairing.
  Tracked::live = 0;
  {
    auto tv = makeUniqueHimemVector<Tracked>();
    tv->emplace_back(1);
    tv->emplace_back(2);
    HT_CHECK(Tracked::live == 2, "2 Tracked objects in himemUniqueVector");
    HT_CHECK(IS_PSRAM(tv.get()), "Tracked vector object itself is in PSRAM");
    HT_CHECK(IS_PSRAM(tv->data()), "Tracked element storage is in PSRAM");
  }
  HT_CHECK(Tracked::live == 0, "all Tracked destructors called when himemUniqueVector resets");
}

// ===========================================================================
// 12. HimemMap<K, V>
// ===========================================================================
static auto testHimemMap() -> void {
  HT_LOG("--- HimemMap<int,int> ---");

  HimemMap<int, int> m;
  HT_CHECK(m.empty(), "default-constructed HimemMap is empty");

  m[1] = 10;
  m[2] = 20;
  m[3] = 30;
  HT_CHECK(m.size() == 3, "size() correct after 3 insertions");

  HT_CHECK(m.count(2) == 1, "count() finds existing key");
  HT_CHECK(m.count(99) == 0, "count() returns 0 for missing key");
  HT_CHECK(m.at(3) == 30, "at() returns correct value");

  // Keys must be stored in sorted order.
  bool sorted = true;
  int prev = std::numeric_limits<int>::min();
  for (auto const &[k, v] : m) {
    if (k <= prev) { sorted = false; break; }
    prev = k;
  }
  HT_CHECK(sorted, "HimemMap iterates keys in sorted order");

  m.erase(2);
  HT_CHECK(m.size() == 2 && m.count(2) == 0, "erase() removes key correctly");

  // String keys.
  HimemMap<HimemString, int> sm;
  sm[HimemString("alpha")] = 1;
  sm[HimemString("beta")]  = 2;
  HT_CHECK(sm.size() == 2, "HimemMap<HimemString,int> size correct");
  HT_CHECK(sm[HimemString("alpha")] == 1, "HimemMap<HimemString,int> lookup correct");

  m.clear();
  HT_CHECK(m.empty(), "clear() empties HimemMap");
}

// ===========================================================================
// 13. HimemUnorderedMap<K, V>
// ===========================================================================
static auto testHimemUnorderedMap() -> void {
  HT_LOG("--- HimemUnorderedMap<int,int> ---");

  HimemUnorderedMap<int, int> m;
  HT_CHECK(m.empty(), "default-constructed HimemUnorderedMap is empty");

  m[10] = 100;
  m[20] = 200;
  m[30] = 300;
  HT_CHECK(m.size() == 3, "size() correct after 3 insertions");

  HT_CHECK(m.count(20) == 1, "count() finds existing key");
  HT_CHECK(m.count(99) == 0, "count() returns 0 for missing key");
  HT_CHECK(m.at(30) == 300, "at() returns correct value");

  m[10] = 999;
  HT_CHECK(m.at(10) == 999, "operator[] overwrites existing key");

  m.erase(20);
  HT_CHECK(m.size() == 2 && m.count(20) == 0, "erase() removes key correctly");

  // Verify all remaining entries are accessible.
  bool ok = (m.count(10) == 1) && (m.count(30) == 1);
  HT_CHECK(ok, "remaining keys accessible after erase");

  // String keys.
  HimemUnorderedMap<HimemString, int> sm;
  sm[HimemString("one")]   = 1;
  sm[HimemString("two")]   = 2;
  sm[HimemString("three")] = 3;
  HT_CHECK(sm.size() == 3, "HimemUnorderedMap<HimemString,int> size correct");
  HT_CHECK(sm[HimemString("two")] == 2, "HimemUnorderedMap<HimemString,int> lookup correct");

  m.clear();
  HT_CHECK(m.empty(), "clear() empties HimemUnorderedMap");
}

} // anonymous namespace

// ===========================================================================
// Public entry point
// ===========================================================================

auto testHimem() -> bool {
  sPass = 0;
  sFail = 0;

  HT_LOG("========== himem test suite start ==========");

  testAllocator();
  testDeleterScalar();
  testDeleterArray();
  testSharedScalar();
  testSharedArray();
  testUniqueScalar();
  testUniqueArray();
  testHimemString();
  testHimemVector();
  testUniqueString();
  testUniqueVector();
  testHimemMap();
  testHimemUnorderedMap();

  HT_LOG("========== himem test suite end: %d passed, %d failed ==========", sPass, sFail);

  return sFail == 0;
}
