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

static int s_pass = 0;
static int s_fail = 0;

// Log a single check result and update counters.
// msg must be a string literal so it can be concatenated into the format string.
#define HT_CHECK(cond, msg)                                                                        \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      HT_LOG("FAIL [%s:%d] " msg, __FILE__, __LINE__);                                             \
      ++s_fail;                                                                                    \
    } else {                                                                                       \
      HT_LOG("PASS " msg);                                                                         \
      ++s_pass;                                                                                    \
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
static void test_allocator() {
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
static void test_deleter_scalar() {
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
static void test_deleter_array() {
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
// 4. make_shared_himem<T> — scalar
// ===========================================================================
static void test_shared_scalar() {
  HT_LOG("--- make_shared_himem<T> scalar ---");

  Tracked::live = 0;
  auto p        = make_shared_himem<Tracked>(7);

  HT_CHECK(p != nullptr, "make_shared_himem returns non-null");
  HT_CHECK(IS_PSRAM(p.get()), "managed object is in PSRAM");
  HT_CHECK(p->value == 7, "object constructed with correct value");
  HT_CHECK(Tracked::live == 1, "one live instance after make_shared_himem");
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
// 5. make_shared_himem<T[]> — unbounded array
// ===========================================================================
static void test_shared_array() {
  HT_LOG("--- make_shared_himem<T[]> unbounded array ---");

  constexpr std::size_t N = 10;
  auto arr                = make_shared_himem<int[]>(N);

  HT_CHECK(arr != nullptr, "make_shared_himem array returns non-null");
  HT_CHECK(IS_PSRAM(arr.get()), "array storage is in PSRAM");

  for (std::size_t i = 0; i < N; ++i) arr[i] = static_cast<int>(i * i);

  bool ok = true;
  for (std::size_t i = 0; i < N; ++i) ok = ok && (arr[i] == static_cast<int>(i * i));
  HT_CHECK(ok, "array elements written and read back correctly");
}

// ===========================================================================
// 6. make_unique_himem<T> — scalar
// ===========================================================================
static void test_unique_scalar() {
  HT_LOG("--- make_unique_himem<T> scalar ---");

  Tracked::live = 0;
  auto u        = make_unique_himem<Tracked>(99);

  HT_CHECK(u != nullptr, "make_unique_himem returns non-null");
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
// 7. make_unique_himem<T[]> — unbounded array
// ===========================================================================
static void test_unique_array() {
  HT_LOG("--- make_unique_himem<T[]> unbounded array ---");

  constexpr std::size_t N = 6;
  auto arr                = make_unique_himem<int[]>(N);

  HT_CHECK(arr != nullptr, "make_unique_himem array returns non-null");
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
// 8. himem_string
// ===========================================================================
static void test_himem_string() {
  HT_LOG("--- himem_string ---");

  himem_string s;
  HT_CHECK(s.empty(), "default-constructed himem_string is empty");

  // Use a string >= 32 chars to guarantee the custom allocator is invoked
  // (bypass potential small-string-optimisation in the platform's libstdc++).
  s = "Hello from PSRAM string buffer!!"; // 32 chars
  HT_CHECK(s == "Hello from PSRAM string buffer!!", "assignment and comparison correct");
  HT_CHECK(s.size() == 32, "size() correct after assignment");
  HT_CHECK(IS_PSRAM(s.data()), "character buffer is in PSRAM");

  s += " Extended.";
  HT_CHECK(s.find("Extended") != himem_string::npos, "append and find work correctly");

  himem_string s2{s}; // copy
  HT_CHECK(s2 == s, "copy-constructed string equals original");
  HT_CHECK(IS_PSRAM(s2.data()), "copy's character buffer is in PSRAM");

  s2.clear();
  HT_CHECK(s2.empty(), "clear() empties string");
  HT_CHECK(!s.empty(), "original unaffected by copy-then-clear");
}

// ===========================================================================
// 9. himem_vector<int>
// ===========================================================================
static void test_himem_vector() {
  HT_LOG("--- himem_vector<int> ---");

  himem_vector<int> v;
  HT_CHECK(v.empty(), "default-constructed himem_vector is empty");

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
    himem_vector<Tracked> tv;
    tv.emplace_back(1);
    tv.emplace_back(2);
    tv.emplace_back(3);
    HT_CHECK(Tracked::live == 3, "3 Tracked objects live in himem_vector");
    HT_CHECK(IS_PSRAM(tv.data()), "Tracked element storage is in PSRAM");
  }
  HT_CHECK(Tracked::live == 0, "all Tracked destructors called when himem_vector leaves scope");
}

// ===========================================================================
// 10. himem_unique_string
// ===========================================================================
static void test_unique_string() {
  HT_LOG("--- himem_unique_string ---");

  auto s = make_unique_himem_string("Hello from PSRAM unique string!!");

  HT_CHECK(s != nullptr, "make_unique_himem_string returns non-null");
  HT_CHECK(IS_PSRAM(s.get()), "himem_string object itself is in PSRAM");
  HT_CHECK(*s == "Hello from PSRAM unique string!!", "content is correct");
  HT_CHECK(IS_PSRAM(s->data()), "character buffer is in PSRAM");

  // Move: ownership must transfer cleanly.
  auto s2 = std::move(s);
  HT_CHECK(!s, "source null after move");
  HT_CHECK(s2 != nullptr, "destination valid after move");
  HT_CHECK(*s2 == "Hello from PSRAM unique string!!", "content preserved after move");
  HT_CHECK(IS_PSRAM(s2.get()), "moved-to object still in PSRAM");

  // Empty / default construction then mutate.
  auto empty = make_unique_himem_string();
  HT_CHECK(empty != nullptr, "empty make_unique_himem_string returns non-null");
  HT_CHECK(empty->empty(), "default-constructed string is empty");
  HT_CHECK(IS_PSRAM(empty.get()), "empty string object is in PSRAM");

  empty->assign("Assigned after default construction!!");
  HT_CHECK(*empty == "Assigned after default construction!!",
           "assign after default-construction works");
}

// ===========================================================================
// 11. himem_unique_vector<T>
// ===========================================================================
static void test_unique_vector() {
  HT_LOG("--- himem_unique_vector<int> ---");

  // Sized construction: 5 elements, each 7.
  auto v = make_unique_himem_vector<int>(5u, 7);

  HT_CHECK(v != nullptr, "make_unique_himem_vector returns non-null");
  HT_CHECK(IS_PSRAM(v.get()), "himem_vector<int> object itself is in PSRAM");
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
  auto ev = make_unique_himem_vector<float>();
  HT_CHECK(ev != nullptr, "empty make_unique_himem_vector returns non-null");
  HT_CHECK(ev->empty(), "default-constructed vector is empty");
  HT_CHECK(IS_PSRAM(ev.get()), "empty vector object itself is in PSRAM");

  // Verify destructor pairing.
  Tracked::live = 0;
  {
    auto tv = make_unique_himem_vector<Tracked>();
    tv->emplace_back(1);
    tv->emplace_back(2);
    HT_CHECK(Tracked::live == 2, "2 Tracked objects in himem_unique_vector");
    HT_CHECK(IS_PSRAM(tv.get()), "Tracked vector object itself is in PSRAM");
    HT_CHECK(IS_PSRAM(tv->data()), "Tracked element storage is in PSRAM");
  }
  HT_CHECK(Tracked::live == 0, "all Tracked destructors called when himem_unique_vector resets");
}

} // anonymous namespace

// ===========================================================================
// Public entry point
// ===========================================================================

bool himem_run_tests() {
  s_pass = 0;
  s_fail = 0;

  HT_LOG("========== himem test suite start ==========");

  test_allocator();
  test_deleter_scalar();
  test_deleter_array();
  test_shared_scalar();
  test_shared_array();
  test_unique_scalar();
  test_unique_array();
  test_himem_string();
  test_himem_vector();
  test_unique_string();
  test_unique_vector();

  HT_LOG("========== himem test suite end: %d passed, %d failed ==========", s_pass, s_fail);

  return s_fail == 0;
}
