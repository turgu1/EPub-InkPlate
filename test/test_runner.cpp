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

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Forward declarations — one per test translation unit.
// ---------------------------------------------------------------------------
auto testHimem()       -> bool;
auto testDom()         -> bool;
auto testSimpleDb()    -> bool;
auto testConfig()      -> bool;
auto testCss()         -> bool;
auto testDisplayList() -> bool;
auto testAppConfig()   -> bool;
auto testEPub()        -> bool;

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main() {
  std::printf("========================================\n");
  std::printf(" EPub-InkPlate Linux Unit-Test Suite\n");
  std::printf("========================================\n\n");

  struct Suite {
    const char *name;
    bool (*run)();
  };

  static const Suite suites[] = {
    { "himem",     testHimem    },
    { "dom",       testDom      },
    { "simple_db", testSimpleDb },
    { "config",       testConfig      },
    { "css",          testCss         },
    { "display_list", testDisplayList },
    { "app_config",   testAppConfig   },
    { "epub",         testEPub        },
  };

  int totalFail = 0;

  for (const auto &s : suites) {
    std::printf("\n===== SUITE: %-12s =====\n", s.name);
    bool ok = s.run();
    std::printf("===== %-12s %s =====\n", s.name, ok ? "PASSED" : "FAILED");
    if (!ok) ++totalFail;
  }

  std::printf("\n========================================\n");
  std::printf("  %s  (%d suite(s) failed)\n",
              totalFail == 0 ? "ALL SUITES PASSED" : "FAILURES DETECTED",
              totalFail);
  std::printf("========================================\n");

  return totalFail == 0 ? 0 : 1;
}
