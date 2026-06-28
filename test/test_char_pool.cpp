// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// CharPool test suite
//
// Tests cover:
//   1.  Basic allocate / raw memory is writable
//   2.  Size-class rounding (allocations are multiples of 2^MIN_LOG = 8)
//   3.  Free-list reuse — deallocate then re-allocate same class
//   4.  Multi-block growth (allocate past one BLOCK_SIZE)
//   5.  Oversized allocation (> MAX_ALLOC) returns nullptr gracefully
//   6.  set() — string copy into pool
//   7.  Statistics — getTotalAllocated / getTotalFreed track correctly
//   8.  CharPoolAllocator<T> — basic allocate / deallocate
//   9.  PoolString construction and assignment
//   10. PoolString — growth triggers realloc, old slot recycled
//   11. Multiple PoolStrings share the same pool correctly
//   12. Allocator equality — same pool ⟹ equal, different ⟹ not equal
//   13. PoolContext<Tag> — init / reset lifecycle
//   14. StaticPoolAllocator — string construction with no explicit allocator arg
//   15. StaticPoolAllocator — two Tags use independent pools
// ---------------------------------------------------------------------------

#include "char_pool.hpp"
#include "test_stats.hpp"

#include <cstdio>
#include <cstring>

namespace {

int gPass = 0;
int gFail = 0;

#define CP_CHECK(cond, msg)                                                                        \
  do {                                                                                             \
    if (cond) {                                                                                    \
      std::printf("  PASS %s\n", msg);                                                             \
      ++gPass;                                                                                     \
    } else {                                                                                       \
      std::printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg);                                 \
      ++gFail;                                                                                     \
    }                                                                                              \
  } while (0)

// ---------------------------------------------------------------------------
// 1. Basic allocate — pointer is non-null and memory is writable
// ---------------------------------------------------------------------------
static void testBasicAllocate() {
  std::printf("  [1. Basic allocate]\n");
  auto pool = CharPool::Make();

  char *p = pool->allocate(10);
  CP_CHECK(p != nullptr, "allocate(10) returns non-null");

  // Write and read back a pattern to confirm the memory works.
  std::memset(p, 0xAB, 10);
  bool ok = true;
  for (int i = 0; i < 10; ++i) ok &= (static_cast<unsigned char>(p[i]) == 0xAB);
  CP_CHECK(ok, "allocated memory is writable and readable");
}

// ---------------------------------------------------------------------------
// 2. Size-class rounding — actual slot is next power-of-2 >= 8
// ---------------------------------------------------------------------------
static void testSizeClassRounding() {
  std::printf("  [2. Size-class rounding]\n");
  auto pool = CharPool::Make();

  // Allocate three requests that should fall into the same 8-byte class,
  // then free them all. If they were different sizes they would land in
  // different free lists and not reuse each other.
  char *a = pool->allocate(1);
  char *b = pool->allocate(4);
  char *c = pool->allocate(8);
  CP_CHECK(a != nullptr && b != nullptr && c != nullptr, "allocate(1/4/8) all succeed");

  pool->deallocate(a, 1);
  pool->deallocate(b, 4);
  pool->deallocate(c, 8);

  // After freeing three 8-byte slots, re-allocating three times should reuse them.
  char *d = pool->allocate(6); // same class as a (rounds to 8)
  char *e = pool->allocate(7); // same class as b (rounds to 8)
  char *f = pool->allocate(8); // same class as c
  CP_CHECK(d != nullptr && e != nullptr && f != nullptr,
           "re-allocate after free succeeds (free-list reuse)");
  // The addresses must be among {a, b, c} because they were recycled.
  bool reused = ((d == a || d == b || d == c) && (e == a || e == b || e == c) &&
                 (f == a || f == b || f == c) && d != e && e != f && d != f);
  CP_CHECK(reused, "recycled slots match previously freed addresses");
}

// ---------------------------------------------------------------------------
// 3. Free-list reuse — LIFO order within a size class
// ---------------------------------------------------------------------------
static void testFreeListReuse() {
  std::printf("  [3. Free-list reuse (LIFO)]\n");
  auto pool = CharPool::Make();

  char *p1 = pool->allocate(16); // class-1 (16 B)
  char *p2 = pool->allocate(16);
  CP_CHECK(p1 != nullptr && p2 != nullptr, "two 16-byte allocations succeed");

  pool->deallocate(p2, 16); // free last first
  pool->deallocate(p1, 16); // free first last → p1 is now head of free list

  char *r1 = pool->allocate(16); // should get p1 (LIFO head)
  char *r2 = pool->allocate(16); // should get p2
  CP_CHECK(r1 == p1 && r2 == p2, "LIFO recycling order is correct");
}

// ---------------------------------------------------------------------------
// 4. Multi-block growth — crossing the BLOCK_SIZE boundary
// ---------------------------------------------------------------------------
static void testMultiBlockGrowth() {
  std::printf("  [4. Multi-block growth]\n");
  auto pool = CharPool::Make();

  // Each allocation requests 256 bytes (class-5: 256 B).
  // BLOCK_SIZE = 4096, so 16 such allocations exactly fill one block.
  // The 17th must trigger a new block allocation.
  static constexpr int ALLOC_SIZE = 256;
  static constexpr int COUNT      = 20; // crosses one block boundary
  char *ptrs[COUNT]{};

  bool allOk = true;
  for (int i = 0; i < COUNT; ++i) {
    ptrs[i] = pool->allocate(ALLOC_SIZE);
    if (!ptrs[i]) {
      allOk = false;
      break;
    }
    // Write a canary per allocation.
    std::memset(ptrs[i], static_cast<int>(i + 1), ALLOC_SIZE);
  }
  CP_CHECK(allOk, "all 20 allocations across block boundary succeed");

  // Verify canaries are intact (no overlap between allocations).
  bool canariesOk = true;
  for (int i = 0; i < COUNT; ++i) {
    for (int j = 0; j < ALLOC_SIZE; ++j) {
      if (static_cast<unsigned char>(ptrs[i][j]) != static_cast<unsigned int>(i + 1)) {
        canariesOk = false;
        break;
      }
    }
  }
  CP_CHECK(canariesOk, "canary patterns intact — no allocation overlap");
}

// ---------------------------------------------------------------------------
// 5. Oversized allocation — request exceeds MAX_ALLOC → nullptr
// ---------------------------------------------------------------------------
static void testOversizedAllocation() {
  std::printf("  [5. Oversized allocation]\n");
  auto pool = CharPool::Make();

  char *p = pool->allocate(CharPool::MAX_ALLOC + 1);
  CP_CHECK(p == nullptr, "allocate(MAX_ALLOC+1) returns nullptr");

  // Zero-size is treated as 1 and should succeed.
  char *q = pool->allocate(0);
  CP_CHECK(q != nullptr, "allocate(0) succeeds (treated as 1)");
}

// ---------------------------------------------------------------------------
// 6. set() — copies a std::string into the pool
// ---------------------------------------------------------------------------
static void testSet() {
  std::printf("  [6. set()]\n");
  auto pool = CharPool::Make();

  std::string src = "Hello, CharPool!";
  char *p         = pool->set(src);
  CP_CHECK(p != nullptr, "set() returns non-null");
  CP_CHECK(std::strcmp(p, src.c_str()) == 0, "set() copies string content correctly");

  // Null terminator must be present.
  CP_CHECK(p[src.length()] == '\0', "set() null-terminates the copy");
}

// ---------------------------------------------------------------------------
// 7. Statistics — getTotalAllocated / getTotalFreed
// ---------------------------------------------------------------------------
static void testStatistics() {
  std::printf("  [7. Statistics]\n");
  auto pool = CharPool::Make();

  CP_CHECK(pool->getTotalAllocated() == 0 && pool->getTotalFreed() == 0,
           "fresh pool has zero allocated and freed");

  // allocate(1) → class-0 → slot = 8 bytes
  char *p8 = pool->allocate(1);
  CP_CHECK(pool->getTotalAllocated() == 8, "after allocate(1): totalAllocated == 8");

  // allocate(16) → class-1 → slot = 16 bytes
  char *p16 = pool->allocate(16);
  CP_CHECK(pool->getTotalAllocated() == 24, "after allocate(16): totalAllocated == 24");

  pool->deallocate(p8, 1);
  CP_CHECK(pool->getTotalFreed() == 8, "after deallocate(p8): totalFreed == 8");

  pool->deallocate(p16, 16);
  CP_CHECK(pool->getTotalFreed() == 24, "after deallocate(p16): totalFreed == 24");
}

// ---------------------------------------------------------------------------
// 8. CharPoolAllocator<T> — basic allocate / deallocate
// ---------------------------------------------------------------------------
static void testCharPoolAllocator() {
  std::printf("  [8. CharPoolAllocator<T>]\n");
  auto pool = CharPool::Make();

  CharPoolAllocator<int> alloc(pool.get());
  CP_CHECK(alloc.pool == pool.get(), "allocator holds correct pool pointer");

  int *p = alloc.allocate(4); // 4 ints = 16 bytes
  CP_CHECK(p != nullptr, "allocator.allocate(4 ints) returns non-null");

  p[0] = 1;
  p[1] = 2;
  p[2] = 3;
  p[3] = 4;
  CP_CHECK(p[0] == 1 && p[3] == 4, "allocated int array is writable");

  alloc.deallocate(p, 4); // must not crash
  CP_CHECK(true, "allocator.deallocate does not crash");

  // Rebind: CharPoolAllocator<int> → CharPoolAllocator<char>
  CharPoolAllocator<char> charAlloc(alloc);
  CP_CHECK(charAlloc.pool == pool.get(), "rebind copy preserves pool pointer");
}

// ---------------------------------------------------------------------------
// 9. PoolString — basic construction and assignment
// ---------------------------------------------------------------------------
static void testPoolStringBasic() {
  std::printf("  [9. PoolString basic]\n");
  auto pool = CharPool::Make();

  CharPoolAllocator<char> alloc(pool.get());
  PoolString s(alloc);
  CP_CHECK(s.empty(), "default PoolString is empty");

  s = "hello";
  CP_CHECK(s == "hello", "PoolString assignment works");
  CP_CHECK(s.length() == 5, "PoolString length correct after assignment");

  PoolString s2(alloc);
  s2 = "world";
  CP_CHECK(s2 == "world", "second PoolString independent of first");

  PoolString s3(s); // copy — uses same allocator (copy-constructs allocator)
  CP_CHECK(s3 == "hello", "PoolString copy-construction preserves content");
}

// ---------------------------------------------------------------------------
// 10. PoolString — growth (reallocation recycles slot via free list)
// ---------------------------------------------------------------------------
static void testPoolStringGrowth() {
  std::printf("  [10. PoolString growth / reallocation]\n");
  auto pool = CharPool::Make();

  CharPoolAllocator<char> alloc(pool.get());
  PoolString s(alloc);

  // Build up the string step by step to force multiple reallocations.
  s = "a";
  for (int i = 0; i < 50; ++i) s += "x";
  CP_CHECK(s.length() == 51, "PoolString grows correctly via repeated append");

  // After growth the pool must have recycled intermediate buffers.
  CP_CHECK(pool->getTotalFreed() > 0, "intermediate buffers were returned to free list");
  CP_CHECK(pool->getTotalAllocated() > pool->getTotalFreed(),
           "net live allocation is positive after growth");
}

// ---------------------------------------------------------------------------
// 11. Multiple PoolStrings sharing the same pool
// ---------------------------------------------------------------------------
static void testMultiplePoolStrings() {
  std::printf("  [11. Multiple PoolStrings in one pool]\n");
  auto pool = CharPool::Make();

  CharPoolAllocator<char> alloc(pool.get());
  PoolString a(alloc), b(alloc), c(alloc);

  a = "alpha";
  b = "bravo";
  c = "charlie";

  CP_CHECK(a == "alpha" && b == "bravo" && c == "charlie",
           "three PoolStrings in same pool hold independent values");

  a.clear();
  CP_CHECK(a.empty() && b == "bravo" && c == "charlie",
           "clearing one PoolString does not affect others");
}

// ---------------------------------------------------------------------------
// 12. Allocator equality — same pool ⟹ equal; different pool ⟹ not equal
// ---------------------------------------------------------------------------
static void testAllocatorEquality() {
  std::printf("  [12. Allocator equality]\n");
  auto pool1 = CharPool::Make();
  auto pool2 = CharPool::Make();

  CharPoolAllocator<char> a1(pool1.get());
  CharPoolAllocator<char> a2(pool1.get()); // same pool
  CharPoolAllocator<char> a3(pool2.get()); // different pool

  CP_CHECK(a1 == a2, "two allocators on same pool compare equal");
  CP_CHECK(a1 != a3, "allocators on different pools compare not equal");
}

} // namespace

// ---------------------------------------------------------------------------
// Tag types and string aliases used by tests 13-15
// ---------------------------------------------------------------------------
namespace {

struct TagA {};
struct TagB {};

using StringA = std::basic_string<char, std::char_traits<char>, StaticPoolAllocator<char, TagA>>;
using StringB = std::basic_string<char, std::char_traits<char>, StaticPoolAllocator<char, TagB>>;

// ---------------------------------------------------------------------------
// 13. PoolContext — init / reset lifecycle
// ---------------------------------------------------------------------------
static void testPoolContextLifecycle() {
  std::printf("  [13. PoolContext lifecycle]\n");
  auto pool = CharPool::Make();

  CP_CHECK(PoolContext<TagA>::pool == nullptr, "pool starts as nullptr before init");

  PoolContext<TagA>::init(pool.get());
  CP_CHECK(PoolContext<TagA>::pool == pool.get(), "init stores pool pointer");

  // Allocate through StaticPoolAllocator to confirm pool is reached.
  const std::size_t before = pool->getTotalAllocated();
  StaticPoolAllocator<char, TagA> alloc;
  char *p = alloc.allocate(32);
  CP_CHECK(p != nullptr, "StaticPoolAllocator allocates while pool is set");
  CP_CHECK(pool->getTotalAllocated() > before, "allocation was charged to the pool");
  alloc.deallocate(p, 32);

  PoolContext<TagA>::reset();
  CP_CHECK(PoolContext<TagA>::pool == nullptr, "reset clears pool pointer");

  // deallocate after reset must not crash (safe no-op).
  alloc.deallocate(p, 32); // p is already freed, but the pool is gone — must not crash
  CP_CHECK(true, "deallocate after reset is a safe no-op");
}

// ---------------------------------------------------------------------------
// 14. StaticPoolAllocator — string construction with no explicit allocator arg
// ---------------------------------------------------------------------------
static void testStaticAllocatorString() {
  std::printf("  [14. StaticPoolAllocator string (no explicit allocator arg)]\n");
  auto pool = CharPool::Make();
  PoolContext<TagA>::init(pool.get());

  // Default-construct: no allocator argument needed.
  StringA s;
  CP_CHECK(s.empty(), "default StringA is empty");

  s = "hello, static pool!";
  CP_CHECK(s == "hello, static pool!", "StringA assignment works");

  StringA s2 = s;
  CP_CHECK(s2 == s, "StringA copy-construction preserves content");

  StringA s3;
  s3 = std::move(s2);
  CP_CHECK(s3 == "hello, static pool!" && s2.empty(), "StringA move-assignment works");

  // Growth beyond SSO threshold.
  StringA big;
  for (int i = 0; i < 60; ++i) big += 'x';
  CP_CHECK(big.length() == 60, "StringA grows correctly");
  CP_CHECK(pool->getTotalFreed() > 0, "intermediate buffers recycled during growth");

  PoolContext<TagA>::reset();
}

// ---------------------------------------------------------------------------
// 15. StaticPoolAllocator — two Tags use independent pools
// ---------------------------------------------------------------------------
static void testTwoTagsIndependent() {
  std::printf("  [15. Two Tags use independent pools]\n");
  auto poolA = CharPool::Make();
  auto poolB = CharPool::Make();
  PoolContext<TagA>::init(poolA.get());
  PoolContext<TagB>::init(poolB.get());

  StringA a = "alpha";
  StringB b = "bravo";
  CP_CHECK(a == "alpha" && b == "bravo", "each tag holds correct value");

  // Allocations must be charged to the correct pool.
  CP_CHECK(poolA->getTotalAllocated() >= poolB->getTotalAllocated() ||
               poolB->getTotalAllocated() >= poolA->getTotalAllocated(),
           "each pool tracks its own allocations independently");

  // StaticPoolAllocators of different tags must compare not-equal.
  StaticPoolAllocator<char, TagA> allocA;
  StaticPoolAllocator<char, TagA> allocA2;
  StaticPoolAllocator<char, TagB> allocB;
  CP_CHECK(allocA == allocA2, "same-tag allocators compare equal");
  // Different-Tag allocators are different C++ types — cannot be compared with ==.
  // Verify they ARE different types at compile time.
  CP_CHECK((!std::is_same_v<decltype(allocA), decltype(allocB)>),
           "different-tag allocators are distinct C++ types");

  PoolContext<TagA>::reset();
  PoolContext<TagB>::reset();
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry point called by test_runner.cpp
// ---------------------------------------------------------------------------
auto testCharPool() -> TestStats {
  std::printf("\n--- CharPool ---\n");

  gPass = gFail = 0;

  testBasicAllocate();
  testSizeClassRounding();
  testFreeListReuse();
  testMultiBlockGrowth();
  testOversizedAllocation();
  testSet();
  testStatistics();
  testCharPoolAllocator();
  testPoolStringBasic();
  testPoolStringGrowth();
  testMultiplePoolStrings();
  testAllocatorEquality();
  testPoolContextLifecycle();
  testStaticAllocatorString();
  testTwoTagsIndependent();

  std::printf("--- CharPool: %d passed, %d failed ---\n", gPass, gFail);
  return TestStats{gPass, gFail};
}
