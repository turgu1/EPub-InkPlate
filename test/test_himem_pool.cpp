// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "himem_pool.hpp"
#include "test_stats.hpp"

#include <cstdio>
#include <string>

namespace {

int gChecks = 0;
int gFails  = 0;

#define HP_CHECK(cond)                                                                             \
  do {                                                                                             \
    ++gChecks;                                                                                     \
    if (!(cond)) {                                                                                 \
      ++gFails;                                                                                    \
      std::printf("  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond);                              \
    }                                                                                              \
  } while (0)

struct Tracked {
  static int live;
  static int ctorCount;
  static int dtorCount;

  int value;

  explicit Tracked(int v = 0) : value(v) {
    ++live;
    ++ctorCount;
  }

  Tracked(const Tracked &other) : value(other.value) {
    ++live;
    ++ctorCount;
  }

  ~Tracked() {
    --live;
    ++dtorCount;
  }
};

int Tracked::live      = 0;
int Tracked::ctorCount = 0;
int Tracked::dtorCount = 0;

auto resetTrackedCounters() -> void {
  Tracked::live      = 0;
  Tracked::ctorCount = 0;
  Tracked::dtorCount = 0;
}

auto testMakeDefaultBlockSize() -> void {
  std::printf("  [Make default block size]\n");

  auto pool = HimemPool<Tracked>::Make();
  HP_CHECK(pool != nullptr);
  HP_CHECK(pool->blockSize() == 20);
  HP_CHECK(pool->blockCount() == 0);
  HP_CHECK(pool->liveCount() == 0);
}

auto testDirectConstruction() -> void {
  std::printf("  [direct stack construction]\n");

  HimemPool<Tracked> pool(3);
  HP_CHECK(pool.blockSize() == 3);
  HP_CHECK(pool.blockCount() == 0);
  HP_CHECK(pool.liveCount() == 0);

  {
    auto a = pool.create(10);
    auto b = pool.create(20);
    HP_CHECK(a != nullptr && b != nullptr);
    HP_CHECK(pool.blockCount() == 1);
    HP_CHECK(pool.liveCount() == 2);
  }

  HP_CHECK(pool.liveCount() == 0);
}

auto testCustomBlockGrowth() -> void {
  std::printf("  [custom block size and growth]\n");

  auto pool = HimemPool<Tracked>::Make(3);
  HP_CHECK(pool != nullptr);
  HP_CHECK(pool->blockSize() == 3);

  auto a = pool->create(1);
  auto b = pool->create(2);
  auto c = pool->create(3);
  HP_CHECK(a != nullptr && b != nullptr && c != nullptr);
  HP_CHECK(pool->blockCount() == 1);
  HP_CHECK(pool->liveCount() == 3);

  auto d = pool->create(4);
  HP_CHECK(d != nullptr);
  HP_CHECK(pool->blockCount() == 2);
  HP_CHECK(pool->liveCount() == 4);
}

auto testFreedSlotReuse() -> void {
  std::printf("  [freed slot reuse]\n");

  auto pool = HimemPool<Tracked>::Make(4);
  HP_CHECK(pool != nullptr);

  auto a = pool->create(11);
  auto b = pool->create(22);
  auto c = pool->create(33);
  HP_CHECK(a != nullptr && b != nullptr && c != nullptr);

  Tracked *oldB = b.get();
  b.reset();
  HP_CHECK(pool->liveCount() == 2);

  auto d = pool->create(44);
  HP_CHECK(d != nullptr);
  HP_CHECK(d.get() == oldB);
  HP_CHECK(pool->liveCount() == 3);
}

auto testCtorDtorPairing() -> void {
  std::printf("  [constructor/destructor pairing]\n");

  resetTrackedCounters();
  {
    auto pool = HimemPool<Tracked>::Make(2);
    HP_CHECK(pool != nullptr);

    {
      auto x = pool->create(7);
      auto y = pool->create(8);
      HP_CHECK(Tracked::live == 2);
      HP_CHECK(pool->liveCount() == 2);
    }

    HP_CHECK(Tracked::live == 0);
    HP_CHECK(pool->liveCount() == 0);
    HP_CHECK(Tracked::ctorCount == 2);
    HP_CHECK(Tracked::dtorCount == 2);
  }

  HP_CHECK(Tracked::live == 0);
}

auto testNonTrivialType() -> void {
  std::printf("  [non-trivial type]\n");

  auto pool = HimemPool<std::string>::Make(2);
  HP_CHECK(pool != nullptr);

  {
    auto s1 = pool->create("hello");
    auto s2 = pool->create("world");
    HP_CHECK(s1 != nullptr && s2 != nullptr);
    HP_CHECK(*s1 == "hello");
    HP_CHECK(*s2 == "world");
    HP_CHECK(pool->liveCount() == 2);
  }

  HP_CHECK(pool->liveCount() == 0);
}

auto testLifetimeContract() -> void {
  std::printf("  [lifetime contract: ptr before pool]\n");

  resetTrackedCounters();

  auto pool = HimemPool<Tracked>::Make(2);
  HP_CHECK(pool != nullptr);

  auto p = pool->create(123);
  HP_CHECK(p != nullptr);
  HP_CHECK(Tracked::live == 1);

  // The per-object deleter captures a raw pointer to the owning pool.
  // This explicitly documents why the pool must outlive all Ptr values.
  HP_CHECK(p.get_deleter().pool == pool.get());

  // Required order: destroy/reset all Ptr first, then destroy the pool.
  p.reset();
  HP_CHECK(Tracked::live == 0);

  pool.reset();
  HP_CHECK(Tracked::live == 0);
}

} // namespace

auto testHimemPoolTest() -> TestStats {
  gChecks = 0;
  gFails  = 0;

  std::printf("[himem_pool_test] start\n");

  testMakeDefaultBlockSize();
  testDirectConstruction();
  testCustomBlockGrowth();
  testFreedSlotReuse();
  testCtorDtorPairing();
  testNonTrivialType();
  testLifetimeContract();

  std::printf("[himem_pool_test] checks=%d fails=%d\n", gChecks, gFails);
  return TestStats{gChecks - gFails, gFails};
}
