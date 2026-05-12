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
auto testCss() -> TestStats;
auto testDisplayList() -> TestStats;
auto testAppConfig() -> TestStats;
auto testEPub() -> TestStats;
auto testUnzip() -> TestStats;
auto testSimpleList() -> TestStats;
auto testCharPool() -> TestStats;
auto testFontsCache() -> TestStats;
auto testFontsCacheStress() -> TestStats;
auto testGifDecoder() -> TestStats;
auto testSvgDecoder() -> TestStats;

// ---------------------------------------------------------------------------
// Entry point
//
// With no arguments all suites are run.
// Pass one or more suite names as arguments to run only those suites, e.g.:
//   ./build_test/epub_test himem css dom
// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
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
      {"css", testCss},
      {"display_list", testDisplayList},
      {"app_config", testAppConfig},
      {"epub", testEPub},
      {"unzip", testUnzip},
      {"simple_list", testSimpleList},
      {"char_pool", testCharPool},
      {"fonts_cache", testFontsCache},
      {"fonts_cache_stress", testFontsCacheStress},
      {"gif_decoder", testGifDecoder},
      {"svg_decoder", testSvgDecoder},
  };

  // Determine which suites to run. When no arguments are given, run all.
  // Unknown names print a warning and are skipped.
  const bool filterMode = (argc > 1);
  if (filterMode) {
    // Validate provided names early so the user gets a clear error.
    for (int a = 1; a < argc; ++a) {
      bool found = false;
      for (const auto &s : suites) {
        if (std::strcmp(argv[a], s.name) == 0) {
          found = true;
          break;
        }
      }
      if (!found) {
        std::fprintf(stderr, "Unknown suite: \"%s\"\n", argv[a]);
        std::fprintf(stderr, "Available suites:");
        for (const auto &s : suites) std::fprintf(stderr, " %s", s.name);
        std::fprintf(stderr, "\n");
        return 2;
      }
    }
  }

  std::array<SuiteResult, std::size(suites)> results{};
  std::size_t resultCount = 0;

  int failedSuites = 0;
  int totalPassed  = 0;
  int totalFailed  = 0;

  for (std::size_t i = 0; i < std::size(suites); ++i) {
    const auto &s = suites[i];

    // Skip suites not listed on the command line when filtering.
    if (filterMode) {
      bool selected = false;
      for (int a = 1; a < argc; ++a) {
        if (std::strcmp(argv[a], s.name) == 0) {
          selected = true;
          break;
        }
      }
      if (!selected) continue;
    }

    std::printf("\n===== SUITE: %-12s =====\n", s.name);
    const TestStats stats  = s.run();
    results[resultCount++] = SuiteResult{s.name, stats};

    const bool ok = stats.ok();
    std::printf("===== %-12s %s (pass=%d fail=%d) =====\n", s.name, ok ? "PASSED" : "FAILED",
                stats.passed, stats.failed);

    if (!ok) ++failedSuites;
    totalPassed += stats.passed;
    totalFailed += stats.failed;
  }

  std::sort(results.begin(), results.begin() + resultCount,
            [](const SuiteResult &a, const SuiteResult &b) {
    if (a.stats.failed != b.stats.failed) return a.stats.failed > b.stats.failed;
    if (a.stats.passed != b.stats.passed) return a.stats.passed < b.stats.passed;
    return std::strcmp(a.name, b.name) < 0;
  });

  std::printf("\n=============== PER-SUITE STATS ==================\n");
  for (std::size_t i = 0; i < resultCount; ++i) {
    const auto &r = results[i];
    std::printf("  %-16s pass=%4d  fail=%3d  total=%4d\n", r.name, r.stats.passed, r.stats.failed,
                r.stats.total());
  }
  std::printf("==================================================\n");
  std::printf("  GRAND TOTAL: pass=%d  fail=%d  total=%d\n", totalPassed, totalFailed,
              totalPassed + totalFailed);

  std::printf("\n===========================================\n");
  std::printf("  %s  (%d suite(s) failed)\n",
              failedSuites == 0 ? "ALL SUITES PASSED" : "FAILURES DETECTED", failedSuites);
  std::printf("===========================================\n");

  return failedSuites == 0 ? 0 : 1;
}
