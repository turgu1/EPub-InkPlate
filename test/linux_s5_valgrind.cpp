// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test/linux_s5_valgrind.cpp — Headless Linux simulation of regression S5
//                               (concurrent page navigation + mid-flight
//                               pageLocs restart) for Valgrind leak analysis.
//
// Build:  make DEBUG=1 valgrind_s5
// Run:    build_valgrind/epub_valgrind_s5 [/path/to/book.epub]
//
// Under Valgrind:
//   valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all \
//            --num-callers=30 --error-exitcode=1 \
//            build_valgrind/epub_valgrind_s5 [/path/to/book.epub]
//
// The binary runs S5 for a single epub file then exits cleanly so that
// Valgrind can report any still-reachable / definitely-lost allocations.
// ---------------------------------------------------------------------------

// Must be compiled with EPUB_LINUX_BUILD=1 (Makefile sets this).

#define __GLOBAL__ 1 // emit global singleton definitions from headers
#include "global.hpp"

#include "config.hpp"
#include "fonts.hpp"
#include "models/epub.hpp"
#include "models/page_locs.hpp"

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <thread>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
static constexpr int NAV_THREAD_COUNT               = 2;
static constexpr int64_t RESTART_TRIGGER_MS         = 50'000;  ///< trigger restart after 50 s
static constexpr int64_t COMPLETION_TIMEOUT_MS      = 600'000; ///< 10 min hard timeout
static constexpr int64_t NAV_SLEEP_MS               = 50;      ///< nav thread yield interval
static constexpr int64_t PREFLIGHT_START_TIMEOUT_MS = 30'000;
static constexpr int64_t PREFLIGHT_STOP_TIMEOUT_MS  = 15'000;

// ---------------------------------------------------------------------------
// Nav thread
// ---------------------------------------------------------------------------
struct NavCtx {
  std::atomic<bool> stop{false};
  std::atomic<int> navCount{0};
  std::atomic<int> nullCount{0};
  PageId currentPage{0, 0};
  bool forwardOnly{false}; ///< when true, never call getPrevPageId (avoids backward-ASAP cascade)
};

static void navThreadFn(NavCtx *ctx) {
  PageId cur = ctx->currentPage;
  while (!ctx->stop.load(std::memory_order_relaxed)) {
    // Avoid queue-consumer races during teardown: once pageLocs reports completion,
    // navigation threads should stop making PageLocs requests.
    if (pageLocs.isComputationCompleted()) {
      break;
    }

    // In forwardOnly mode always advance forward so that backward navigation
    // past the cleared pagesMap boundary cannot pull the retriever into a
    // repeated-interrupt cascade under Valgrind slowness.
    bool forward       = ctx->forwardOnly || ((ctx->navCount.load() % 2) == 0);
    const PageId *next = forward ? pageLocs.getNextPageId(cur, 1) : pageLocs.getPrevPageId(cur, 1);
    if (next) {
      cur              = *next;
      ctx->currentPage = cur;
      ctx->navCount.fetch_add(1, std::memory_order_relaxed);
    } else {
      ctx->nullCount.fetch_add(1, std::memory_order_relaxed);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(NAV_SLEEP_MS));
  }
}

// ---------------------------------------------------------------------------
// Helper: start / stop nav threads
// ---------------------------------------------------------------------------
static std::thread g_navThreads[NAV_THREAD_COUNT];

static void startNavThreads(NavCtx ctx[], int16_t seedItemref, bool forwardOnly = false) {
  for (int i = 0; i < NAV_THREAD_COUNT; i++) {
    ctx[i].stop.store(false);
    ctx[i].currentPage = PageId{seedItemref, 0};
    ctx[i].forwardOnly = forwardOnly;
    g_navThreads[i]    = std::thread(navThreadFn, &ctx[i]);
  }
}

static void stopNavThreads(NavCtx ctx[]) {
  for (int i = 0; i < NAV_THREAD_COUNT; i++) ctx[i].stop.store(true, std::memory_order_relaxed);
  for (int i = 0; i < NAV_THREAD_COUNT; i++)
    if (g_navThreads[i].joinable()) g_navThreads[i].join();
}

// ---------------------------------------------------------------------------
// Preflight: regression guard for PageLocs lock interaction
//
// Stress getPageNbr() calls while stopping an active control task. If stop
// does not complete within the watchdog timeout, abort the process so CI/test
// harnesses report a hard failure rather than hanging forever.
// ---------------------------------------------------------------------------
static auto runStopUnderPageNbrPollingPreflight(EPubPtr &epub) -> bool {
  std::fprintf(stderr, "[S5 Valgrind] preflight: stopControlTask under getPageNbr polling\n");

  pageLocs.checkForFormatChanges(epub, 0, true);

  int64_t waitedMs = 0;
  while (!pageLocs.isComputationCompleted() && (pageLocs.getGeneratedPageEntryCount() == 0) &&
         (waitedMs < PREFLIGHT_START_TIMEOUT_MS)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    waitedMs += 100;
  }

  if (pageLocs.getGeneratedPageEntryCount() == 0) {
    std::fprintf(stderr,
                 "[S5 Valgrind] preflight failed: no generated pages after %" PRIi64 " ms\n",
                 waitedMs);
    pageLocs.stopControlTask();
    pageLocs.clear();
    return false;
  }

  std::atomic<bool> stopReader{false};
  std::thread reader([&]() {
    PageId p{0, 0};
    while (!stopReader.load(std::memory_order_relaxed)) {
      (void)pageLocs.getPageNbr(p);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  std::atomic<bool> stopDone{false};
  std::thread stopper([&]() {
    pageLocs.stopControlTask();
    stopDone.store(true, std::memory_order_release);
  });

  int64_t stopWaitMs = 0;
  while (!stopDone.load(std::memory_order_acquire) && (stopWaitMs < PREFLIGHT_STOP_TIMEOUT_MS)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stopWaitMs += 50;
  }

  if (!stopDone.load(std::memory_order_acquire)) {
    std::fprintf(stderr,
                 "[S5 Valgrind] preflight DEADLOCK: stopControlTask did not complete after %" PRIi64
                 " ms\n",
                 stopWaitMs);
    std::fflush(stderr);
    std::_Exit(2);
  }

  if (stopper.joinable()) stopper.join();
  stopReader.store(true, std::memory_order_relaxed);
  if (reader.joinable()) reader.join();

  pageLocs.clear();
  std::fprintf(stderr, "[S5 Valgrind] preflight: OK\n");
  return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
auto main(int argc, char **argv) -> int {
  // Default to a lightweight book so the test runs in a reasonable time.
  const char *epubPath = (argc > 1) ? argv[1]
                                    : "/home/turgu1/Dev/EPub-InkPlate/SDCard/books/"
                                      "Austen, Jane - Pride and Prejudice.epub";

  std::fprintf(stderr, "[S5 Valgrind] epub: %s\n", epubPath);

  // ------------------------------------------------------------------
  // Initialise globals (mirrors Linux main() in src/main.cpp)
  // ------------------------------------------------------------------
  if (!config.read()) {
    std::fprintf(stderr, "[S5 Valgrind] WARNING: config.read() failed — fonts may not load\n");
  }

  if (!appFonts.setup()) {
    std::fprintf(stderr, "[S5 Valgrind] FATAL: appFonts.setup() failed — cannot continue\n");
    return 1;
  }

  // ------------------------------------------------------------------
  // Open epub (this is the main test's EPub, NOT pageLocsInstance)
  // ------------------------------------------------------------------
  auto epub = EPub::Make();
  if (!epub) {
    std::fprintf(stderr, "[S5 Valgrind] FATAL: EPub::Make() failed\n");
    return 1;
  }
  if (!epub->open(HimemString(epubPath))) {
    std::fprintf(stderr, "[S5 Valgrind] FATAL: epub->open() failed for %s\n", epubPath);
    return 1;
  }

  if (!runStopUnderPageNbrPollingPreflight(epub)) {
    std::fprintf(stderr, "[S5 Valgrind] FATAL: preflight regression failed\n");
    return 1;
  }

  // ------------------------------------------------------------------
  // Save initial format params; prepare a modified copy for restart
  // ------------------------------------------------------------------
  EPub::BookFormatParams initialFmt = *epub->getBookFormatParams();
  EPub::BookFormatParams restartFmt = initialFmt;
  restartFmt.showTitle              = (initialFmt.showTitle == 0) ? 1 : 0;

  // ------------------------------------------------------------------
  // Start pageLocs computation and nav threads
  // ------------------------------------------------------------------
  pageLocs.checkForFormatChanges(epub, 0, true);

  NavCtx navCtx[NAV_THREAD_COUNT];
  for (int i = 0; i < NAV_THREAD_COUNT; i++) {
    navCtx[i].navCount.store(0);
    navCtx[i].nullCount.store(0);
  }
  startNavThreads(navCtx, 0);

  // ------------------------------------------------------------------
  // Monitor loop
  // ------------------------------------------------------------------
  bool restarted    = false;
  bool completed    = false;
  bool started      = false;
  int64_t elapsedMs = 0;
  int64_t nextLogMs = 10'000;

  while (!completed) {
    int16_t itemref    = pageLocs.getCurrentItemrefIndex();
    uint32_t pageCount = pageLocs.getGeneratedPageEntryCount();

    if (!started && (itemref >= 0 || pageCount > 0)) {
      started = true;
      std::fprintf(stderr, "[S5 Valgrind] pageLocs computation %s\n",
                   restarted ? "restarted" : "started");
    }

    // Trigger mid-flight restart once
    if (started && !restarted && !pageLocs.isComputationCompleted() &&
        elapsedMs >= RESTART_TRIGGER_MS) {

      std::fprintf(stderr,
                   "[S5 Valgrind] restarting after %" PRIi64 " ms from item=%" PRIi16
                   " (showTitle: %d -> %d)\n",
                   elapsedMs, itemref, (int)initialFmt.showTitle, (int)restartFmt.showTitle);

      stopNavThreads(navCtx);
      pageLocs.stopControlTask();

      *epub->getBookFormatParams() = restartFmt;
      int16_t restartItem          = (itemref >= 0) ? itemref : 0;
      pageLocs.checkForFormatChanges(epub, restartItem, true);

      restarted = true;
      started   = false;
      elapsedMs = 0;
      nextLogMs = 10'000;

      // Do NOT restart nav threads post-restart.
      //
      // After checkForFormatChanges the entire pagesMap is cleared.  Nav
      // threads starting at item 0 would trigger retrieveAsap() for items
      // 0, 1, 2, … (each costing a 5 s timeout) while the retriever
      // sequentially recomputes from restartItem onwards — they perpetually
      // interrupt each other and drag completion to a standstill.
      //
      // Concurrent ASAP stress is already exercised pre-restart.  The
      // post-restart phase just needs to verify that computation finishes
      // correctly with no crashes or memory leaks.
      continue;
    }

    if (started && pageLocs.isComputationCompleted()) {
      completed = true;
      std::fprintf(stderr, "[S5 Valgrind] computation done — pages=%" PRIu32 "\n", pageCount);
      break;
    }

    if (elapsedMs >= COMPLETION_TIMEOUT_MS) {
      std::fprintf(stderr, "[S5 Valgrind] TIMEOUT after %" PRIi64 " ms\n", elapsedMs);
      break;
    }

    if (elapsedMs >= nextLogMs) {
      int32_t totalNav = 0;
      for (int i = 0; i < NAV_THREAD_COUNT; i++)
        totalNav += navCtx[i].navCount.load() + navCtx[i].nullCount.load();
      std::fprintf(stderr,
                   "[S5 Valgrind] %s item=%" PRIi16 " pages=%" PRIu32 " nav=%" PRIi32
                   " elapsed=%" PRIi64 " ms\n",
                   restarted ? "[post-restart]" : "", itemref, pageCount, totalNav, elapsedMs);
      nextLogMs += 10'000;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    elapsedMs += 200;
  }

  // ------------------------------------------------------------------
  // Clean up — this is where leaks must NOT appear
  // ------------------------------------------------------------------
  // Stop nav threads first so they cannot consume mgrQueue replies intended
  // for stopControlTask() (single-consumer queue semantics).
  stopNavThreads(navCtx);
  pageLocs.stopControlTask();

  pageLocs.clear();
  epub->closeFile();
  epub.reset(); // explicitly destroy before appFonts cleanup

  appFonts.clearEverything();

  std::fprintf(stderr, "[S5 Valgrind] cleanup done — Valgrind should report no leaks\n");
  return completed ? 0 : 1;
}
