// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// EPub-InkPlate Linux unit-test runner
//
// Each test suite lives in its own translation unit (test_himem.cpp, …) and
// exports a single bool-returning function following the naming convention
//   auto test<Suite>() -> bool;
//
// A return value of true means all checks in that suite passed.
// ---------------------------------------------------------------------------

#define __GLOBAL__ 1
#include "global.hpp"
#include "test_stats.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Forward declarations — one per test translation unit.
// ---------------------------------------------------------------------------
auto testHimem() -> TestStats;
auto testHimemPoolTest() -> TestStats;
auto testDom() -> TestStats;
auto testSimpleDb() -> TestStats;
auto testConfig() -> TestStats;
auto testCss() -> TestStats;
auto testDisplayList() -> TestStats;
auto testAppConfig() -> TestStats;
auto testEPub() -> TestStats;
auto testSimpleList() -> TestStats;

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main() {
  std::printf("========================================\n");
  std::printf(" EPub-InkPlate Linux Unit-Test Suite\n");
  std::printf("========================================\n\n");

  struct Suite {
    const char *name;
    TestStats (*run)();
  };

  struct SuiteResult {
    const char *name;
    TestStats stats;
  };

  static const Suite suites[] = {
      {"himem", testHimem},
      {"himem_pool_test", testHimemPoolTest},
      {"dom", testDom},
      {"simple_db", testSimpleDb},
      {"config", testConfig},
      {"css", testCss},
      {"display_list", testDisplayList},
      {"app_config", testAppConfig},
      {"epub", testEPub},
      {"simple_list", testSimpleList},
  };

  std::array<SuiteResult, std::size(suites)> results{};

  int failedSuites = 0;
  int totalPassed  = 0;
  int totalFailed  = 0;

  for (std::size_t i = 0; i < std::size(suites); ++i) {
    const auto &s = suites[i];
    std::printf("\n===== SUITE: %-12s =====\n", s.name);
    const TestStats stats = s.run();
    results[i]            = SuiteResult{s.name, stats};

    const bool ok = stats.ok();
    std::printf("===== %-12s %s (pass=%d fail=%d) =====\n", s.name, ok ? "PASSED" : "FAILED",
                stats.passed, stats.failed);

    if (!ok) ++failedSuites;
    totalPassed += stats.passed;
    totalFailed += stats.failed;
  }

  std::sort(results.begin(), results.end(), [](const SuiteResult &a, const SuiteResult &b) {
    if (a.stats.failed != b.stats.failed) return a.stats.failed > b.stats.failed;
    if (a.stats.passed != b.stats.passed) return a.stats.passed < b.stats.passed;
    return std::strcmp(a.name, b.name) < 0;
  });

  std::printf("\n=============== PER-SUITE STATS ===============\n");
  for (const auto &r : results) {
    std::printf("  %-16s pass=%4d  fail=%3d  total=%4d\n", r.name, r.stats.passed, r.stats.failed,
                r.stats.total());
  }
  std::printf("===============================================\n");
  std::printf("  GRAND TOTAL: pass=%d  fail=%d  total=%d\n", totalPassed, totalFailed,
              totalPassed + totalFailed);

  std::printf("\n========================================\n");
  std::printf("  %s  (%d suite(s) failed)\n",
              failedSuites == 0 ? "ALL SUITES PASSED" : "FAILURES DETECTED", failedSuites);
  std::printf("========================================\n");

  return failedSuites == 0 ? 0 : 1;
}
