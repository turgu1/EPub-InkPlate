// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "himem_test.hpp"
#include "himem.hpp"

#include <cstring>

#if EPUB_INKPLATE_BUILD
  #include "esp_log.h"
  #include "esp_memory_utils.h"
  static const char *const TAG = "himem_test";
  #define HT_LOG(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
  #define IS_PSRAM(ptr) esp_ptr_external_ram(static_cast<const void *>(ptr))
#else
  #include <cstdio>
  #define HT_LOG(fmt, ...) std::printf("[himem_test] " fmt "\n", ##__VA_ARGS__)
  #define IS_PSRAM(ptr) true // no SPIRAM on host — skip address checks
#endif

// ===========================================================================
// Internal helpers
// ===========================================================================

namespace {

static int sPass = 0;
static int sFail = 0;

// Log a single check result and update counters.
// msg must be a string literal so it can be concatenated into the format string.
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
  HT_LOG("--- makeUniqueHimem<T[]> unbounded array ---");

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
// 8. himemString
// ===========================================================================
static auto testHimemString() -> void {
  HT_LOG("--- himemString ---");

  himemString s;
  HT_CHECK(s.empty(), "default-constructed himemString is empty");

  // Use a string >= 32 chars to guarantee the custom allocator is invoked
  // (bypass potential small-string-optimisation in the platform's libstdc++).
  s = "Hello from PSRAM string buffer!!"; // 32 chars
  HT_CHECK(s == "Hello from PSRAM string buffer!!", "assignment and comparison correct");
  HT_CHECK(s.size() == 32, "size() correct after assignment");
  HT_CHECK(IS_PSRAM(s.data()), "character buffer is in PSRAM");

  s += " Extended.";
  HT_CHECK(s.find("Extended") != himemString::npos, "append and find work correctly");

  himemString s2{s}; // copy
  HT_CHECK(s2 == s, "copy-constructed string equals original");
  HT_CHECK(IS_PSRAM(s2.data()), "copy's character buffer is in PSRAM");

  s2.clear();
  HT_CHECK(s2.empty(), "clear() empties string");
  HT_CHECK(!s.empty(), "original unaffected by copy-then-clear");
}

// ===========================================================================
// 9. himemVector<int>
// ===========================================================================
static auto testHimemVector() -> void {
  HT_LOG("--- himemVector<int> ---");

  himemVector<int> v;
  HT_CHECK(v.empty(), "default-constructed himemVector is empty");

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
    himemVector<Tracked> tv;
    tv.emplace_back(1);
    tv.emplace_back(2);
    tv.emplace_back(3);
    HT_CHECK(Tracked::live == 3, "3 Tracked objects live in himemVector");
    HT_CHECK(IS_PSRAM(tv.data()), "Tracked element storage is in PSRAM");
  }
  HT_CHECK(Tracked::live == 0, "all Tracked destructors called when himemVector leaves scope");
}

// ===========================================================================
// 10. himemUniqueString
// ===========================================================================
static auto testUniqueString() -> void {
  HT_LOG("--- himemUniqueString ---");

  auto s = makeUniqueHimemString("Hello from PSRAM unique string!!");

  HT_CHECK(s != nullptr, "makeUniqueHimemString returns non-null");
  HT_CHECK(IS_PSRAM(s.get()), "himemString object itself is in PSRAM");
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
  HT_CHECK(IS_PSRAM(v.get()), "himemVector<int> object itself is in PSRAM");
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

} // anonymous namespace

// ===========================================================================
// Public entry point
// ===========================================================================

auto himemRunTests() -> bool {
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

  HT_LOG("========== himem test suite end: %d passed, %d failed ==========", sPass, sFail);

  return sFail == 0;
}
