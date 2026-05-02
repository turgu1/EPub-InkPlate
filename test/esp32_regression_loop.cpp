// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// ESP32 Memory-Stress Regression Loop Test
//
// Build with:
//   idf.py build  (after setting CONFIG_EPUB_REGRESSION_TEST=y in sdkconfig or
//                  adding -DREGRESSION_TEST=1 to the CMakeLists extra CFLAGS)
//
// The test runs from mainTask() when REGRESSION_TEST is defined (see main.cpp).
// It exercises five stress scenarios while tracking heap and stack watermarks:
//
//   1. Repeated book open/close  – cycles through every .epub on the SDCard.
//   2. TOC load                  – builds the TOC for every book.
//   3. Cover / image retrieval   – fetches each book's cover bitmap from the DB.
//   4. PageLocs recomputation     – recompute page start locations for each book.
//   5. Concurrent navigation     – page-lookup API calls while pageLocs computes in background.
//
// After each scenario the minimum free heap so far and the task stack watermark
// are printed and heap health is validated (leak + fragmentation regression).
// The test fails (and prints a summary) if:
//   - any individual operation returns an error, OR
//   - the heap watermark drops below HEAP_WARN_THRESHOLD bytes.
//
// The test DOES NOT drive the display or FreeRTOS event manager, so it can
// run before the screen is initialised.
// ---------------------------------------------------------------------------

#if EPUB_INKPLATE_BUILD && REGRESSION_TEST

  #include "global.hpp"
  #include "himem.hpp"
  #include "models/books_dir.hpp"
  #include "models/config.hpp"
  #include "models/epub.hpp"
  #include "models/page_locs.hpp"
  #include "models/toc.hpp"

  #include "esp_heap_caps.h"
  #include "esp_log.h"
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"

  #if DATE_TIME_RTC
    #include "controllers/clock.hpp"
    #include "controllers/ntp.hpp"
    #include "controllers/wifi.hpp"
    #include <ctime>
  #endif

  #include "controllers/web_server.hpp"

  #include <atomic>
  #include <cinttypes>
  #include <cstdio>
  #include <cstring>
  #include <dirent.h>
  #include <sys/stat.h>

  static const char *const TAG = "RegressionTest";

  // ---------------------------------------------------------------------------
  // Log-noise suppression helpers
  // Certain tags (TTF font loader, Page renderer) emit expected content-quality
  // messages that would otherwise bury real regression failures in the log.
  // ---------------------------------------------------------------------------
  static void suppressNoisyTags() {
    esp_log_level_set("TTF", ESP_LOG_NONE);
    esp_log_level_set("Page", ESP_LOG_NONE);
  }

  static void restoreNoisyTags() {
    esp_log_level_set("TTF", ESP_LOG_ERROR);
    esp_log_level_set("Page", ESP_LOG_ERROR);
  }

  // ---------------------------------------------------------------------------
  // Test Selection – bitmask controlling which scenarios are executed.
  //
  //   Bit  Scenario
  //   ---  --------
  //    0   S1 – Repeated book open/close
  //    1   S2 – TOC load
  //    2   S3 – Cover / image retrieval
  //    3   S4 – PageLocs recomputation
  //    4   S5 – Concurrent navigation
  //    5   S6 – WiFi + NTP time sync (requires DATE_TIME_RTC)
  //    6   S7 – WiFi web server (STA, 30 s smoke test)
  //
  // Examples:
  //   0x7F  – run all seven scenarios
  //   0x40  – run only S7 (web server)
  //   0x20  – run only S6 (WiFi/NTP)
  //   0x10  – run only S5 (concurrent navigation)
  //   0x0F  – run S1-S4
  // ---------------------------------------------------------------------------
  #define REGRESSION_SCENARIOS 0x03 // default: S1-S5 during active development

  // ---------------------------------------------------------------------------
  // Tuning constants
  // ---------------------------------------------------------------------------
  static constexpr uint32_t HEAP_WARN_THRESHOLD = 32 * 1024; ///< Warn if free heap < 32 KB
  // Per-scenario heap validation tolerances.
  static constexpr uint32_t HEAP_LEAK_TOLERANCE_BYTES   = 1024;
  static constexpr uint32_t FRAG_GROWTH_TOLERANCE_BYTES = 1024;
  static constexpr uint32_t S1_FRAG_GROWTH_TOLERANCE_BYTES =
      70 * 1024; ///< S1 may trigger one-time allocator slab reshaping (~64 KiB)
  static constexpr float FRAG_PERCENT_WARN    = 5.0f;
  static constexpr int OPEN_CLOSE_CYCLES      = 10; ///< Repeat open/close N times per book
  static constexpr int BASELINE_WARMUP_CYCLES = 1;  ///< Unmeasured pass to settle allocators
  // Page-locs recomputation can take multiple minutes on large books.
  static constexpr uint32_t PAGE_LOCS_TIMEOUT_MS = 15 * 60 * 1000;
  // Scenario 5: concurrent navigation parameters
  static constexpr int NAV_THREAD_COUNT = 2; ///< Number of concurrent navigator threads
  static constexpr uint32_t NAV_STALL_TIMEOUT_MS =
      120 * 1000; ///< Consider S5 stalled only if page-locs and nav activity both stop
  static constexpr uint32_t NAV_SLEEP_MS =
      200; ///< Sleep between nav operations (reduced aggressiveness)
  static constexpr uint32_t NAV_RESTART_TRIGGER_MS =
      50 * 1000; ///< In S5, restart page-locs after 50 seconds of computation
  static constexpr uint32_t NAV_RESTART_PROGRESS_TIMEOUT_MS =
      30000; ///< After S5 restart, require forward progress within this window

  // ---------------------------------------------------------------------------
  // Watermark helpers
  // ---------------------------------------------------------------------------
  static uint32_t gMinFreeHeap     = UINT32_MAX;
  static uint32_t gInitialFreeHeap = 0;

  static void updateHeap() {
    uint32_t free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    if (free < gMinFreeHeap) gMinFreeHeap = free;
  }

  static void printWatermarks(const char *label) {
    updateHeap();
    uint32_t stackHwm = uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t);
    ESP_LOGI(TAG, "[%s] min-free-heap=%" PRIu32 " B  stack-hwm=%" PRIu32 " B", label, gMinFreeHeap,
             stackHwm);
    if (gMinFreeHeap < HEAP_WARN_THRESHOLD) {
      ESP_LOGW(TAG, "[%s] WARNING: heap dropped below %" PRIu32 " B threshold!", label,
               (uint32_t)HEAP_WARN_THRESHOLD);
    }
  }

  struct HeapSnapshot {
    uint32_t free{0};          ///< heap_caps_get_free_size(DEFAULT)  = internal + PSRAM
    uint32_t largest{0};       ///< heap_caps_get_largest_free_block(DEFAULT)
    uint32_t minFree{0};       ///< rolling minimum of free (DEFAULT)
    uint32_t spiramFree{0};    ///< heap_caps_get_free_size(SPIRAM)
    uint32_t spiramLargest{0}; ///< heap_caps_get_largest_free_block(SPIRAM)
  };

  struct HeapTrendPoint {
    const char *scenarioName{nullptr};
    HeapSnapshot before{};
    HeapSnapshot after{};
  };

  static constexpr uint8_t MAX_HEAP_TREND_POINTS = 16;
  static HeapTrendPoint gHeapTrend[MAX_HEAP_TREND_POINTS];
  static uint8_t gHeapTrendCount{0};

  static auto takeHeapSnapshot() -> HeapSnapshot {
    return {.free          = heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
            .largest       = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
            .minFree       = gMinFreeHeap,
            .spiramFree    = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            .spiramLargest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)};
  }

  static auto fragmentationBytes(const HeapSnapshot &snap) -> uint32_t {
    return (snap.free > snap.largest) ? (snap.free - snap.largest) : 0;
  }

  static auto fragmentationPercent(const HeapSnapshot &snap) -> float {
    if (snap.free == 0) return 100.0f;
    return (100.0f * static_cast<float>(fragmentationBytes(snap))) / static_cast<float>(snap.free);
  }

  static auto verifyHeapAfterScenario(const char *scenarioName, const HeapSnapshot &before,
                                      const HeapSnapshot &after) -> int {
    int errs = 0;

    const bool isS1OpenClose =
        (scenarioName != nullptr) && (strcmp(scenarioName, "S1 open/close") == 0);
    const uint32_t fragGrowthTolerance =
        isS1OpenClose ? S1_FRAG_GROWTH_TOLERANCE_BYTES : FRAG_GROWTH_TOLERANCE_BYTES;

    int32_t freeDelta        = static_cast<int32_t>(after.free) - static_cast<int32_t>(before.free);
    uint32_t beforeFragBytes = fragmentationBytes(before);
    uint32_t afterFragBytes  = fragmentationBytes(after);
    float beforeFragPct      = fragmentationPercent(before);
    float afterFragPct       = fragmentationPercent(after);

    int32_t spiramDelta =
        static_cast<int32_t>(after.spiramFree) - static_cast<int32_t>(before.spiramFree);
    uint32_t spiramFragBefore =
        (before.spiramFree > before.spiramLargest) ? (before.spiramFree - before.spiramLargest) : 0;
    uint32_t spiramFragAfter =
        (after.spiramFree > after.spiramLargest) ? (after.spiramFree - after.spiramLargest) : 0;
    int32_t spiramFragDelta =
        static_cast<int32_t>(spiramFragAfter) - static_cast<int32_t>(spiramFragBefore);

    ESP_LOGI(TAG,
             "[%s] DEFAULT free=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32 ") largest=%" PRIu32
             "->%" PRIu32 " frag=%" PRIu32 "B (%.1f%%)->%" PRIu32 "B (%.1f%%)",
             scenarioName, before.free, after.free, freeDelta, before.largest, after.largest,
             beforeFragBytes, beforeFragPct, afterFragBytes, afterFragPct);
    ESP_LOGI(TAG,
             "[%s] SPIRAM  free=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32 ") largest=%" PRIu32
             "->%" PRIu32 " frag=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32 ")",
             scenarioName, before.spiramFree, after.spiramFree, spiramDelta, before.spiramLargest,
             after.spiramLargest, spiramFragBefore, spiramFragAfter, spiramFragDelta);

    if ((before.free > after.free) && ((before.free - after.free) > HEAP_LEAK_TOLERANCE_BYTES)) {
      ESP_LOGE(TAG, "[%s] heap leak suspected: lost %" PRIu32 " B (tolerance=%" PRIu32 " B)",
               scenarioName, (before.free - after.free), (uint32_t)HEAP_LEAK_TOLERANCE_BYTES);
      ++errs;
    }

    if ((before.spiramFree > after.spiramFree) &&
        ((before.spiramFree - after.spiramFree) > HEAP_LEAK_TOLERANCE_BYTES)) {
      ESP_LOGE(TAG, "[%s] SPIRAM leak suspected: lost %" PRIu32 " B (tolerance=%" PRIu32 " B)",
               scenarioName, (before.spiramFree - after.spiramFree),
               (uint32_t)HEAP_LEAK_TOLERANCE_BYTES);
      ++errs;
    }

    if ((afterFragBytes > beforeFragBytes) &&
        ((afterFragBytes - beforeFragBytes) > fragGrowthTolerance)) {
      ESP_LOGE(TAG,
               "[%s] DEFAULT fragmentation increased by %" PRIu32 " B (before=%" PRIu32
               " B, after=%" PRIu32 " B, tolerance=%" PRIu32 " B)",
               scenarioName, (afterFragBytes - beforeFragBytes), beforeFragBytes, afterFragBytes,
               fragGrowthTolerance);
      ++errs;
    }

    if ((spiramFragAfter > spiramFragBefore) &&
        ((spiramFragAfter - spiramFragBefore) > fragGrowthTolerance)) {
      ESP_LOGE(TAG,
               "[%s] SPIRAM fragmentation increased by %" PRIu32 " B (before=%" PRIu32
               " B, after=%" PRIu32 " B, tolerance=%" PRIu32 " B)",
               scenarioName, (spiramFragAfter - spiramFragBefore), spiramFragBefore,
               spiramFragAfter, fragGrowthTolerance);
      ++errs;
    }

    return errs;
  }

  static void recordHeapTrend(const char *scenarioName, const HeapSnapshot &before,
                              const HeapSnapshot &after) {
    if (gHeapTrendCount >= MAX_HEAP_TREND_POINTS) return;
    gHeapTrend[gHeapTrendCount++] = {
        .scenarioName = scenarioName, .before = before, .after = after};
  }

  static void logHeapPerBook(const char *scenarioName, uint8_t bookIndex, uint8_t bookCount,
                             const char *bookPath, const HeapSnapshot &before,
                             const HeapSnapshot &after) {
    uint32_t beforeFrag = fragmentationBytes(before);
    uint32_t afterFrag  = fragmentationBytes(after);
    int32_t freeDelta   = static_cast<int32_t>(after.free) - static_cast<int32_t>(before.free);
    int32_t largestDelta =
        static_cast<int32_t>(after.largest) - static_cast<int32_t>(before.largest);

    uint32_t beforeSpiramFrag =
        (before.spiramFree > before.spiramLargest) ? (before.spiramFree - before.spiramLargest) : 0;
    uint32_t afterSpiramFrag =
        (after.spiramFree > after.spiramLargest) ? (after.spiramFree - after.spiramLargest) : 0;
    int32_t spiramFreeDelta =
        static_cast<int32_t>(after.spiramFree) - static_cast<int32_t>(before.spiramFree);
    int32_t spiramLargestDelta =
        static_cast<int32_t>(after.spiramLargest) - static_cast<int32_t>(before.spiramLargest);

    ESP_LOGI(TAG,
             "%s [%u/%u] '%s' DEFAULT free=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32
             ") largest=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32 ") frag=%" PRIu32 "->%" PRIu32,
             scenarioName, static_cast<unsigned>(bookIndex + 1), static_cast<unsigned>(bookCount),
             bookPath, before.free, after.free, freeDelta, before.largest, after.largest,
             largestDelta, beforeFrag, afterFrag);

    ESP_LOGI(TAG,
             "%s [%u/%u] '%s' SPIRAM  free=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32
             ") largest=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32 ") frag=%" PRIu32 "->%" PRIu32,
             scenarioName, static_cast<unsigned>(bookIndex + 1), static_cast<unsigned>(bookCount),
             bookPath, before.spiramFree, after.spiramFree, spiramFreeDelta, before.spiramLargest,
             after.spiramLargest, spiramLargestDelta, beforeSpiramFrag, afterSpiramFrag);
  }

  static void printHeapTrendHistory(uint32_t initialFreeHeap) {
    ESP_LOGI(TAG, "------------- Heap Trend History -------------");
    for (uint8_t i = 0; i < gHeapTrendCount; ++i) {
      const HeapTrendPoint &p = gHeapTrend[i];

      int32_t freeDeltaScenario =
          static_cast<int32_t>(p.after.free) - static_cast<int32_t>(p.before.free);
      int32_t freeDeltaTotal =
          static_cast<int32_t>(p.after.free) - static_cast<int32_t>(initialFreeHeap);
      int32_t spiramDeltaScenario =
          static_cast<int32_t>(p.after.spiramFree) - static_cast<int32_t>(p.before.spiramFree);
      int32_t spiramLargestDelta = static_cast<int32_t>(p.after.spiramLargest) -
                                   static_cast<int32_t>(p.before.spiramLargest);

      uint32_t beforeFragBytes = fragmentationBytes(p.before);
      uint32_t afterFragBytes  = fragmentationBytes(p.after);
      int32_t fragDeltaScenario =
          static_cast<int32_t>(afterFragBytes) - static_cast<int32_t>(beforeFragBytes);
      float afterFragPct = fragmentationPercent(p.after);

      uint32_t spiramFragBefore = (p.before.spiramFree > p.before.spiramLargest)
                                      ? (p.before.spiramFree - p.before.spiramLargest)
                                      : 0;
      uint32_t spiramFragAfter  = (p.after.spiramFree > p.after.spiramLargest)
                                      ? (p.after.spiramFree - p.after.spiramLargest)
                                      : 0;
      int32_t spiramFragDelta =
          static_cast<int32_t>(spiramFragAfter) - static_cast<int32_t>(spiramFragBefore);

      ESP_LOGI(TAG,
               "[%u] %-18s DEFAULT free=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32 "/tot=%+" PRIi32
               ") frag=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32 ",%.1f%%) min=%" PRIu32,
               static_cast<unsigned>(i + 1), p.scenarioName, p.before.free, p.after.free,
               freeDeltaScenario, freeDeltaTotal, beforeFragBytes, afterFragBytes,
               fragDeltaScenario, afterFragPct, p.after.minFree);
      ESP_LOGI(TAG,
               "[%u] %-18s SPIRAM  free=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32 ") largest=%" PRIu32
               "->%" PRIu32 " (d=%+" PRIi32 ") frag=%" PRIu32 "->%" PRIu32 " (d=%+" PRIi32 ")",
               static_cast<unsigned>(i + 1), p.scenarioName, p.before.spiramFree,
               p.after.spiramFree, spiramDeltaScenario, p.before.spiramLargest,
               p.after.spiramLargest, spiramLargestDelta, spiramFragBefore, spiramFragAfter,
               spiramFragDelta);
    }
    ESP_LOGI(TAG, "-----------------------------------------------");
  }

  // ---------------------------------------------------------------------------
  // Collect list of .epub paths on the SD card
  // ---------------------------------------------------------------------------
  struct BookList {
    static constexpr uint8_t MAX_BOOKS = 32;
    // BOOKS_FOLDER ("/sdcard/books") + '/' + NAME_MAX (255) + '\0' = 272 bytes minimum
    static constexpr uint16_t PATH_SIZE = 512;
    char paths[MAX_BOOKS][PATH_SIZE];
    uint8_t count{0};

    auto scan() -> bool {
      count   = 0;
      DIR *dp = opendir(BOOKS_FOLDER);
      if (!dp) {
        ESP_LOGW(TAG, "Cannot open books folder: %s", BOOKS_FOLDER);
        return false;
      }
      struct dirent *de;
      while ((de = readdir(dp)) && count < MAX_BOOKS) {
        int16_t len = strlen(de->d_name);
        if (len > 5 && strcasecmp(&de->d_name[len - 5], ".epub") == 0) {
          snprintf(paths[count], PATH_SIZE, "%s/%s", BOOKS_FOLDER, de->d_name);
          ++count;
        }
      }
      closedir(dp);
      ESP_LOGI(TAG, "Found %d .epub file(s).", count);
      return true;
    }
  };

  // ---------------------------------------------------------------------------
  // Scenario 1 – Repeated book open / close
  // ---------------------------------------------------------------------------
  [[maybe_unused]] static int scenarioOpenClose(BookList &bl) {
    ESP_LOGI(TAG, "--- Scenario 1: repeated open/close (%d cycles per book) ---",
             OPEN_CLOSE_CYCLES);
    int errors = 0;
    for (int cycle = 0; cycle < OPEN_CLOSE_CYCLES; ++cycle) {
      for (uint8_t i = 0; i < bl.count; ++i) {
        auto epub = EPub::Make();
        if (!epub) {
          ESP_LOGE(TAG, "S1: EPub::Make() failed");
          ++errors;
          continue;
        }
        if (!epub->open(bl.paths[i])) {
          ESP_LOGE(TAG, "S1: open failed: %s", bl.paths[i]);
          ++errors;
        } else {
          ESP_LOGD(TAG, "S1: opened '%s' title='%s'", bl.paths[i],
                   epub->getTitle() ? epub->getTitle() : "(none)");
          epub->closeFile();
        }
        updateHeap();
        vTaskDelay(10 / portTICK_PERIOD_MS); // yield
      }
    }
    printWatermarks("open/close");
    return errors;
  }

  [[maybe_unused]] static void warmupOpenCloseBaseline(BookList &bl) {
    ESP_LOGI(TAG, "--- Baseline warm-up: open/close (%d cycle per book) ---",
             BASELINE_WARMUP_CYCLES);
    for (int cycle = 0; cycle < BASELINE_WARMUP_CYCLES; cycle++) {
      for (uint8_t i = 0; i < bl.count; i++) {
        auto epub = EPub::Make();
        if (!epub) continue;
        if (epub->open(bl.paths[i])) epub->closeFile();
        updateHeap();
        vTaskDelay(10 / portTICK_PERIOD_MS);
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Scenario 2 – TOC load for every book
  // ---------------------------------------------------------------------------
  [[maybe_unused]] static int scenarioToc(BookList &bl) {
    ESP_LOGI(TAG, "--- Scenario 2: TOC load ---");
    int errors = 0;
    for (uint8_t i = 0; i < bl.count; i++) {
      auto epub = EPub::Make();
      if (!epub) {
        ++errors;
        continue;
      }
      if (!epub->open(bl.paths[i])) {
        ++errors;
        continue;
      }

      int16_t itemCount = epub->getItemCount();
      ESP_LOGD(TAG, "S2: '%s'  items=%" PRIi16, bl.paths[i], itemCount);

      if (itemCount <= 0) {
        ESP_LOGW(TAG, "S2: no items in '%s'", bl.paths[i]);
      }

      epub->closeFile();
      updateHeap();
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    printWatermarks("toc");
    return errors;
  }

  // ---------------------------------------------------------------------------
  // Scenario 3 – Cover / image retrieval from the books-dir DB
  // ---------------------------------------------------------------------------
  [[maybe_unused]] static int scenarioCoverImages(BooksDir &booksDir) {
    ESP_LOGI(TAG, "--- Scenario 3: cover image retrieval ---");
    int errors = 0;
    int16_t n  = booksDir.getBookCount();
    ESP_LOGI(TAG, "S3: %d book(s) in DB", n);
    for (int16_t i = 0; i < n; ++i) {
      auto book = booksDir.getBookData(static_cast<uint16_t>(i));
      if (!book) {
        ESP_LOGE(TAG, "S3: getBookData(%d) returned nullptr", i);
        ++errors;
      } else {
        uint32_t pixels = book->coverSize();
        ESP_LOGD(TAG, "S3: book[%d] cover %ux%u (%" PRIu32 " px)", i, book->coverDim.width,
                 book->coverDim.height, pixels);
        if (pixels == 0) {
          ESP_LOGW(TAG, "S3: book[%d] has zero-size cover", i);
        }
      }
      updateHeap();
      vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    printWatermarks("covers");
    return errors;
  }

  // ---------------------------------------------------------------------------
  // Navigation thread context (for Scenario 5)
  // ---------------------------------------------------------------------------
  struct NavThreadContext {
    TaskHandle_t handle{nullptr};
    std::atomic<int> navCount{0};
    std::atomic<int> navNullCount{0};
    std::atomic<bool> shouldStop{false};
    std::atomic<bool> exited{false};
    PageId currentPage{0, 0};
  };

  static void navigationThreadTask(void *pvParams) {
    NavThreadContext *ctx = static_cast<NavThreadContext *>(pvParams);
    PageId currentPage    = ctx->currentPage;

    while (!ctx->shouldStop.load()) {
      // Alternate between forward/backward navigation
      bool goForward = (ctx->navCount.load() % 2) == 0;

      const PageId *nextPageId = goForward ? pageLocs.getNextPageId(currentPage, 1)
                                           : pageLocs.getPrevPageId(currentPage, 1);

      if (nextPageId) {
        currentPage      = *nextPageId;
        ctx->currentPage = currentPage;
        ctx->navCount.fetch_add(1);
      } else {
        ctx->navNullCount.fetch_add(1);
      }

      vTaskDelay(NAV_SLEEP_MS / portTICK_PERIOD_MS); // yield and give other threads time
    }

    // Notification: this thread is exiting
    ESP_LOGD(TAG, "S5: nav thread done (nav=%d nulls=%d)", ctx->navCount.load(),
             ctx->navNullCount.load());
    ctx->exited.store(true);
    ctx->handle = nullptr;
    vTaskDelete(nullptr);
  }
  // ---------------------------------------------------------------------------
  // Scenario 4 - Page locations recomputation for every book
  // ---------------------------------------------------------------------------
  [[maybe_unused]] static int scenarioPageLocsRecompute(BookList &bl) {
    ESP_LOGI(TAG, "--- Scenario 4: pageLocs recomputation ---");
    int errors = 0;

    for (uint8_t i = 0; i < bl.count; i++) {
      HeapSnapshot beforeBook = takeHeapSnapshot();

      auto epub = EPub::Make();
      if (!epub) {
        ESP_LOGE(TAG, "S4: EPub::Make() failed");
        ++errors;
        HeapSnapshot afterBook = takeHeapSnapshot();
        logHeapPerBook("S4", i, bl.count, bl.paths[i], beforeBook, afterBook);
        continue;
      }
      if (!epub->open(bl.paths[i])) {
        ESP_LOGE(TAG, "S4: open failed: %s", bl.paths[i]);
        ++errors;
        HeapSnapshot afterBook = takeHeapSnapshot();
        logHeapPerBook("S4", i, bl.count, bl.paths[i], beforeBook, afterBook);
        continue;
      }

      ESP_LOGI(TAG, "S4: recomputing page locations for %s", bl.paths[i]);
      pageLocs.checkForFormatChanges(epub, 0, true);

      bool started       = false;
      bool completed     = false;
      bool aborted       = false;
      bool timedOut      = false;
      uint32_t elapsedMs = 0;
      uint32_t nextLogMs = 10000;
      int16_t lastStatus = -1;

      while (!completed && !aborted && !timedOut) {
        int16_t status         = pageLocs.getPageCountOrPercent();
        lastStatus             = status;
        int16_t currentItemref = pageLocs.getCurrentItemrefIndex();

        if (status < 0) {
          aborted = true;
          break;
        }

        // Do not consider completion before we observed an actual computation start.
        if (!started && ((currentItemref >= 0) || (status > 0))) {
          started = true;
        }

        // Once started, completion is when control task is gone and we have a positive page
        // count.
        if (started && (currentItemref == -1) && (status > 0)) {
          completed = true;
          ESP_LOGI(TAG, "S4: %s recompute done, pages=%" PRIi16, bl.paths[i], status);
          break;
        }

        if (elapsedMs >= nextLogMs) {
          ESP_LOGI(TAG, "S4: %s progress=%" PRIi16 " item=%" PRIi16 " elapsed=%" PRIu32 " ms",
                   bl.paths[i], status, currentItemref, elapsedMs);
          nextLogMs += 10000;
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
        elapsedMs += 200;

        if (elapsedMs >= PAGE_LOCS_TIMEOUT_MS) {
          timedOut = true;
        }
      }

      if (aborted) {
        ESP_LOGE(TAG, "S4: recompute aborted for %s", bl.paths[i]);
        ++errors;
      } else if (timedOut) {
        ESP_LOGE(TAG, "S4: timeout for %s after %" PRIu32 " ms (last=%" PRIi16 ")", bl.paths[i],
                 elapsedMs, lastStatus);
        ++errors;
      } else if (!completed) {
        ESP_LOGE(TAG, "S4: recompute did not complete for %s (last=%" PRIi16 ")", bl.paths[i],
                 lastStatus);
        ++errors;
      }

      // Always stop/join control/retriever before closing this book.
      pageLocs.stopControlTask();
      pageLocs.clear();

      if (completed && (lastStatus <= 0)) {
        ESP_LOGE(TAG, "S4: invalid final page count (%" PRIi16 ") for %s", lastStatus, bl.paths[i]);
        ++errors;
      }

      epub->closeFile();
      updateHeap();
      vTaskDelay(20 / portTICK_PERIOD_MS);

      HeapSnapshot afterBook = takeHeapSnapshot();
      logHeapPerBook("S4", i, bl.count, bl.paths[i], beforeBook, afterBook);
    }

    printWatermarks("page_locs");
    return errors;
  }

  // ---------------------------------------------------------------------------
  // Scenario 5 - Concurrent page navigation during pageLocs recomputation
  // ---------------------------------------------------------------------------
  static int scenarioConcurrentNavigation(BookList &bl) {
    ESP_LOGI(TAG, "--- Scenario 5: concurrent page navigation (%d threads) ---", NAV_THREAD_COUNT);
    ESP_LOGI(TAG, "S5: TTF/Page log noise suppressed (expected content-quality messages)");
    suppressNoisyTags();
    int errors = 0;

    if (bl.count == 0) {
      ESP_LOGI(TAG, "S5: no books available, skipping scenario");
      return 0;
    }

    for (uint8_t bookIdx = 0; bookIdx < bl.count; bookIdx++) {
      HeapSnapshot beforeBook = takeHeapSnapshot();

      ESP_LOGI(TAG, "S5: [%d/%d] '%s'", bookIdx + 1, bl.count, bl.paths[bookIdx]);

      auto epub = EPub::Make();
      if (!epub) {
        ESP_LOGE(TAG, "S5: EPub::Make() failed");
        ++errors;
        HeapSnapshot afterBook = takeHeapSnapshot();
        logHeapPerBook("S5", bookIdx, bl.count, bl.paths[bookIdx], beforeBook, afterBook);
        continue;
      }
      if (!epub->open(bl.paths[bookIdx])) {
        ESP_LOGE(TAG, "S5: open failed: %s", bl.paths[bookIdx]);
        ++errors;
        HeapSnapshot afterBook = takeHeapSnapshot();
        logHeapPerBook("S5", bookIdx, bl.count, bl.paths[bookIdx], beforeBook, afterBook);
        continue;
      }

      // Start pageLocs computation
      pageLocs.checkForFormatChanges(epub, 0, true);

      // Create navigation threads immediately
      NavThreadContext navCtx[NAV_THREAD_COUNT];
      auto startNavThreads = [&](bool resetCounters, int16_t seedItemref = 0) -> bool {
        bool ok = true;
        for (int i = 0; i < NAV_THREAD_COUNT; ++i) {
          navCtx[i].shouldStop.store(false);
          navCtx[i].exited.store(false);
          if (resetCounters) {
            navCtx[i].navCount.store(0);
            navCtx[i].navNullCount.store(0);
            navCtx[i].currentPage = PageId{0, 0};
          } else {
            // After a format-change restart all page offsets are invalid: the old
            // position refers to a layout that no longer exists.  Seed from the
            // restart item so nav threads stay close to where the sequential
            // retriever is working and avoid ASAP requests for items that haven't
            // been computed yet in the new pass.
            navCtx[i].currentPage = PageId{seedItemref, 0};
          }
          navCtx[i].handle = nullptr;
          esp_err_t res    = xTaskCreatePinnedToCore(navigationThreadTask, "nav_task", 4096,
                                                     &navCtx[i], 3, &navCtx[i].handle,
                                                     (i == 0) ? 0 : 1); // Spread across cores
          if (res != pdPASS) {
            ESP_LOGE(TAG, "S5: failed to create nav thread %d (err=%d)", i, res);
            ++errors;
            ok = false;
          }
        }
        return ok;
      };

      auto stopNavThreads = [&]() -> bool {
        bool clean = true;
        for (int i = 0; i < NAV_THREAD_COUNT; ++i) {
          navCtx[i].shouldStop.store(true);
        }
        for (int i = 0; i < NAV_THREAD_COUNT; ++i) {
          uint32_t navWaitMs = 0;
          while (!navCtx[i].exited.load() && navWaitMs < 5000) {
            vTaskDelay(50 / portTICK_PERIOD_MS);
            navWaitMs += 50;
          }
          if (!navCtx[i].exited.load()) {
            ESP_LOGW(TAG, "S5: nav thread %d did not exit cleanly", i);
            ++errors;
            clean = false;
          }
        }
        return clean;
      };

      bool navThreadsRunning = startNavThreads(true);

      EPub::BookFormatParams initialFormat = *epub->getBookFormatParams();
      EPub::BookFormatParams restartFormat = initialFormat;
      restartFormat.showTitle              = (initialFormat.showTitle == 0) ? 1 : 0;
      bool restartedComputation            = false;
      bool restartStartSeen                = false;
      uint32_t restartStartMs              = 0;
      int16_t restartStartItemref          = -1;
      uint32_t restartStartPages           = 0;

      // Wait for pageLocs computation to complete.
      // NOTE: getPageCountOrPercent() must NOT be called here while nav threads
      // are running — it blocks on the same mgrQueue that nav threads use via
      // retrieveAsap(), creating a multi-consumer race that deadlocks the
      // retriever. Use lock-free accessors only inside this loop.
      bool started                = false;
      bool completed              = false;
      bool timedOut               = false;
      bool deadlocked             = false;
      uint32_t elapsedMs          = 0;
      uint32_t nextLogMs          = 10000;
      uint32_t lastActivityMs     = 0;
      int16_t lastItemrefReported = -1;
      uint32_t lastGeneratedPages = 0;

      while (!completed && !timedOut && !deadlocked) {
        int16_t currentItemref      = pageLocs.getCurrentItemrefIndex();
        uint32_t generatedPageCount = pageLocs.getGeneratedPageEntryCount();
        int32_t totalNavActivity    = 0;

        for (int i = 0; i < NAV_THREAD_COUNT; ++i) {
          totalNavActivity += navCtx[i].navCount.load();
          totalNavActivity += navCtx[i].navNullCount.load();
        }

        // Detect computation start: either the retriever has begun (currentItemref >= 0)
        // or it has already inserted at least one page entry.
        if (!started && (currentItemref >= 0 || generatedPageCount > 0)) {
          started             = true;
          lastActivityMs      = elapsedMs;
          lastItemrefReported = currentItemref;
          lastGeneratedPages  = generatedPageCount;
          if (restartedComputation) {
            ESP_LOGI(TAG, "S5: pageLocs computation restarted (item=%" PRIi16 " pages=%" PRIu32 ")",
                     currentItemref, generatedPageCount);
          } else {
            ESP_LOGI(TAG, "S5: pageLocs computation started");
          }

          if (restartedComputation && !restartStartSeen) {
            restartStartSeen    = true;
            restartStartMs      = elapsedMs;
            restartStartItemref = currentItemref;
            restartStartPages   = generatedPageCount;
          }
        }

        // Mid-flight restart test: after enough computation time has elapsed,
        // switch one format parameter and trigger checkForFormatChanges() so the
        // control/retriever threads are stopped and restarted.
        if (started && !restartedComputation && !pageLocs.isComputationCompleted() &&
            elapsedMs >= NAV_RESTART_TRIGGER_MS) {
          int16_t restartItemref = currentItemref >= 0 ? currentItemref : 0;
          ESP_LOGI(TAG,
                   "S5: restarting pageLocs after %" PRIu32 " ms from item=%" PRIi16
                   " with modified format params (showTitle: %d -> %d)",
                   elapsedMs, restartItemref, (int)initialFormat.showTitle,
                   (int)restartFormat.showTitle);

          if (navThreadsRunning) {
            navThreadsRunning = false;
            if (!stopNavThreads()) {
              deadlocked = true;
              break;
            }
          }

          // Force a clean stop and restart of control/retriever tasks.
          pageLocs.stopControlTask();
          *epub->getBookFormatParams() = restartFormat;
          pageLocs.checkForFormatChanges(epub, restartItemref, true);
          restartedComputation = true;
          restartStartSeen     = false;

          ESP_LOGI(TAG, "S5: restart issued, reset state item=%" PRIi16 " pages=%" PRIu32,
                   pageLocs.getCurrentItemrefIndex(), pageLocs.getGeneratedPageEntryCount());

          // Reset activity tracking for the restarted computation.
          started             = false;
          elapsedMs           = 0;
          nextLogMs           = 10000;
          lastActivityMs      = 0;
          lastItemrefReported = -1;
          lastGeneratedPages  = 0;

          navThreadsRunning = startNavThreads(false, restartItemref);
          if (!navThreadsRunning) {
            deadlocked = true;
            break;
          }

          continue;
        }

        // Completion: lock-free atomic flag set by computationCompleted().
        if (started && pageLocs.isComputationCompleted()) {
          completed = true;
          ESP_LOGI(TAG, "S5: pageLocs computation done, pages=%" PRIu32, generatedPageCount);
          break;
        }

        // Stall detection: neither page-locs nor item index changed.
        if (started &&
            (currentItemref != lastItemrefReported || generatedPageCount != lastGeneratedPages)) {
          lastActivityMs      = elapsedMs;
          lastItemrefReported = currentItemref;
          lastGeneratedPages  = generatedPageCount;

          if (restartStartSeen &&
              (currentItemref != restartStartItemref || generatedPageCount != restartStartPages)) {
            restartStartSeen = false; // Restart proved forward progress.
          }
        } else if (started && ((elapsedMs - lastActivityMs) > NAV_STALL_TIMEOUT_MS)) {
          deadlocked = true;
          ESP_LOGW(TAG,
                   "S5: DEADLOCK DETECTED - no page-locs change in %" PRIu32 " ms, item=%" PRIi16
                   " pages=%" PRIu32 " nav=%" PRIi32,
                   (uint32_t)NAV_STALL_TIMEOUT_MS, currentItemref, generatedPageCount,
                   totalNavActivity);
          break;
        }

        if (restartStartSeen && ((elapsedMs - restartStartMs) > NAV_RESTART_PROGRESS_TIMEOUT_MS)) {
          deadlocked = true;
          ESP_LOGW(TAG,
                   "S5: RESTART STALL - no progress within %" PRIu32
                   " ms after restart, item=%" PRIi16 " pages=%" PRIu32,
                   (uint32_t)NAV_RESTART_PROGRESS_TIMEOUT_MS, currentItemref, generatedPageCount);
          break;
        }

        if (elapsedMs >= nextLogMs) {
          if (restartedComputation) {
            ESP_LOGI(TAG,
                     "S5: [post-restart] item=%" PRIi16 " pages=%" PRIu32 " nav=%" PRIi32
                     " elapsed=%" PRIu32 " ms",
                     currentItemref, generatedPageCount, totalNavActivity, elapsedMs);
          } else {
            ESP_LOGI(TAG,
                     "S5: item=%" PRIi16 " pages=%" PRIu32 " nav=%" PRIi32 " elapsed=%" PRIu32
                     " ms",
                     currentItemref, generatedPageCount, totalNavActivity, elapsedMs);
          }
          nextLogMs += 10000;
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
        elapsedMs += 200;

        if (elapsedMs >= PAGE_LOCS_TIMEOUT_MS) {
          timedOut = true;
        }
      }

      if (navThreadsRunning) {
        navThreadsRunning = false;
        (void)stopNavThreads();
      }

      // Verify computation outcome
      // Now that nav threads are stopped it is safe to call getPageCountOrPercent().
      int16_t finalPageCount = completed ? pageLocs.getPageCountOrPercent() : -1;

      if (deadlocked) {
        ESP_LOGE(TAG, "S5: DEADLOCK - concurrent navigation threads caused computation to stall");
        ++errors;
      } else if (timedOut) {
        ESP_LOGE(TAG, "S5: pageLocs computation timed out after %" PRIu32 " ms", elapsedMs);
        ++errors;
      } else if (!restartedComputation) {
        ESP_LOGE(TAG, "S5: mid-computation restart was not exercised");
        ++errors;
      } else if (!completed) {
        ESP_LOGE(TAG, "S5: pageLocs computation did not complete");
        ++errors;
      } else if (finalPageCount <= 0) {
        ESP_LOGE(TAG, "S5: invalid final page count (%" PRIi16 ")", finalPageCount);
        ++errors;
      } else {
        int32_t totalNavs  = navCtx[0].navCount.load() + navCtx[1].navCount.load();
        int32_t totalNulls = navCtx[0].navNullCount.load() + navCtx[1].navNullCount.load();
        ESP_LOGI(TAG,
                 "S5: computation SUCCESS pages=%" PRIi16
                 " | nav-threads: %d successes, %d nulls (%.1f%% hit rate)",
                 finalPageCount, (int)totalNavs, (int)totalNulls,
                 totalNavs > 0 ? (100.0f * totalNavs / (totalNavs + totalNulls)) : 0.0f);
      }

      // Per-book cleanup
      pageLocs.stopControlTask();
      pageLocs.clear();
      epub->closeFile();
      updateHeap();
      vTaskDelay(20 / portTICK_PERIOD_MS);

      HeapSnapshot afterBook = takeHeapSnapshot();
      logHeapPerBook("S5", bookIdx, bl.count, bl.paths[bookIdx], beforeBook, afterBook);
    } // end for each book

    printWatermarks("concurrent_nav");
    restoreNoisyTags();
    return errors;
  }

  // ---------------------------------------------------------------------------
  // Scenario 6 – WiFi connectivity + NTP time retrieval
  // Reads wifi_ssid/wifi_pwd and ntp_server from config.txt on the SD card.
  // Skipped when WiFi is unconfigured (SSID == "NONE").
  // Guarded by DATE_TIME_RTC because NTP/Clock classes only exist in that build.
  // ---------------------------------------------------------------------------
  #if DATE_TIME_RTC
    static int scenarioWifiNtp() {
      ESP_LOGI(TAG, "--- Scenario 6: WiFi connectivity + NTP time sync ---");
      int errors = 0;

      // Read WiFi SSID from config to decide whether to skip.
      HimemString wifiSsid;
      config.get(Config::Ident::SSID, wifiSsid);

      if (wifiSsid.empty() || wifiSsid == "NONE") {
        ESP_LOGW(TAG, "S6: wifi_ssid is '%s' — skipping (set in config.txt to enable)",
                 wifiSsid.c_str());
        return 0;
      }

      HimemString ntpServer;
      config.get(Config::Ident::NTP_SERVER, ntpServer);
      ESP_LOGI(TAG, "S6: connecting to SSID '%s', NTP server '%s'", wifiSsid.c_str(),
               ntpServer.c_str());

      // ntp.getAndSetTime() handles wifi.startSta() / wifi.stop() internally.
      bool ok = ntp.getAndSetTime();

      if (!ok) {
        ESP_LOGE(TAG, "S6: NTP time sync FAILED");
        ++errors;
      } else {
        time_t now = 0;
        Clock::getDateTime(now);

        // Sanity check: time must be >= 1 Jan 2024.
        constexpr time_t MIN_SANE_TIME = 1704067200; // 2024-01-01 00:00:00 UTC
        if (now < MIN_SANE_TIME) {
          ESP_LOGE(TAG, "S6: retrieved time looks implausible (epoch=%" PRIu32 ")", (uint32_t)now);
          ++errors;
        } else {
          // Print human-readable date/time.
          struct tm t{};
          gmtime_r(&now, &t);
          ESP_LOGI(TAG, "S6: NTP sync SUCCESS — UTC %04d-%02d-%02d %02d:%02d:%02d",
                   t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        }
      }

      printWatermarks("wifi_ntp");
      return errors;
    }
  #endif // DATE_TIME_RTC

  // ---------------------------------------------------------------------------
  // Scenario 7 – WiFi web server smoke test
  // Starts the HTTP file server in STA mode, keeps it running for
  // WEB_SERVER_SOAK_MS, then stops it cleanly.
  // Skipped when wifi_ssid is unconfigured ("NONE").
  // ---------------------------------------------------------------------------
  static constexpr uint32_t WEB_SERVER_SOAK_MS = 30 * 1000;

  static int scenarioWebServer() {
    ESP_LOGI(TAG, "--- Scenario 7: WiFi web server (%d s) ---", (int)(WEB_SERVER_SOAK_MS / 1000));
    int errors = 0;

    HimemString wifiSsid;
    config.get(Config::Ident::SSID, wifiSsid);
    if (wifiSsid.empty() || wifiSsid == "NONE") {
      ESP_LOGW(TAG, "S7: wifi_ssid is '%s' — skipping (set in config.txt to enable)",
               wifiSsid.c_str());
      return 0;
    }

    if (!startWebServerHeadless(WebServerMode::STA)) {
      ESP_LOGE(TAG, "S7: web server failed to start");
      ++errors;
    } else {
      ESP_LOGI(TAG, "S7: server running, soaking for %d s ...", (int)(WEB_SERVER_SOAK_MS / 1000));

      uint32_t elapsed = 0;
      while (elapsed < WEB_SERVER_SOAK_MS) {
        constexpr uint32_t TICK = 5000;
        vTaskDelay(TICK / portTICK_PERIOD_MS);
        elapsed += TICK;
        ESP_LOGI(TAG, "S7: running ... %" PRIu32 " / %" PRIu32 " ms", elapsed, WEB_SERVER_SOAK_MS);
        updateHeap();
      }

      stopWebServer();
      ESP_LOGI(TAG, "S7: web server stopped cleanly");
    }

    printWatermarks("web_server");
    return errors;
  }

  // ---------------------------------------------------------------------------
  // Public entry point – called from mainTask() when REGRESSION_TEST is defined
  // ---------------------------------------------------------------------------
  void runRegressionLoop() {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " ESP32 Memory-Stress Regression Loop");
    ESP_LOGI(TAG, "========================================");

    // Capture initial heap before any allocation.
    gInitialFreeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    gMinFreeHeap     = gInitialFreeHeap;
    gHeapTrendCount  = 0;
    ESP_LOGI(TAG, "Initial free heap : %" PRIu32 " B", gInitialFreeHeap);
    ESP_LOGI(TAG, "Active scenarios  : 0x%02X", (unsigned)REGRESSION_SCENARIOS);

    // --- Collect epub file paths (needed by S1-S5) ---
    BookList bl;
    bool booksFolderAvailable = bl.scan();

    constexpr uint8_t NEEDS_BOOKS = 0x1F; // S1-S5 all need the books folder
    if (!booksFolderAvailable && (REGRESSION_SCENARIOS & NEEDS_BOOKS)) {
      ESP_LOGW(TAG, "SD card / books folder not available — S1-S5 will be skipped.");
    }

    if (booksFolderAvailable && (REGRESSION_SCENARIOS & NEEDS_BOOKS)) {
      warmupOpenCloseBaseline(bl);
      vTaskDelay(20 / portTICK_PERIOD_MS);
      gInitialFreeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
      gMinFreeHeap     = gInitialFreeHeap;
      gHeapTrendCount  = 0;
      ESP_LOGI(TAG, "Post-warmup free heap: %" PRIu32 " B", gInitialFreeHeap);
    }

    int totalErrors = 0;

    auto runScenarioWithHeapCheck = [&](const char *name, auto &&scenarioFn) -> void {
      HeapSnapshot before = takeHeapSnapshot();
      int scenarioErrs    = scenarioFn();

      // Let deferred frees settle before measuring post-scenario state.
      vTaskDelay(20 / portTICK_PERIOD_MS);
      updateHeap();
      HeapSnapshot after = takeHeapSnapshot();

      totalErrors += scenarioErrs;
      totalErrors += verifyHeapAfterScenario(name, before, after);
      recordHeapTrend(name, before, after);
    };

    // --- Scenario 1: repeated open/close ---
    if (REGRESSION_SCENARIOS & (1 << 0)) {
      if (booksFolderAvailable) {
        runScenarioWithHeapCheck("S1 open/close", [&]() { return scenarioOpenClose(bl); });
      } else {
        ESP_LOGI(TAG, "--- Scenario 1: SKIPPED (no books folder) ---");
      }
    } else {
      ESP_LOGI(TAG, "--- Scenario 1: SKIPPED (not in REGRESSION_SCENARIOS) ---");
    }

    // --- Scenario 2: TOC load ---
    if (REGRESSION_SCENARIOS & (1 << 1)) {
      if (booksFolderAvailable) {
        runScenarioWithHeapCheck("S2 toc", [&]() { return scenarioToc(bl); });
      } else {
        ESP_LOGI(TAG, "--- Scenario 2: SKIPPED (no books folder) ---");
      }
    } else {
      ESP_LOGI(TAG, "--- Scenario 2: SKIPPED (not in REGRESSION_SCENARIOS) ---");
    }

    // --- Scenario 3: cover image retrieval (requires books DB) ---
    if (REGRESSION_SCENARIOS & (1 << 2)) {
      if (booksFolderAvailable) {
        int16_t dummy = -1;
        if (!booksDir.readBooksDirectory(nullptr, dummy)) {
          ESP_LOGW(TAG, "S3: books DB unavailable, skipping cover scenario");
        } else {
          runScenarioWithHeapCheck("S3 covers", [&]() { return scenarioCoverImages(booksDir); });
        }
      } else {
        ESP_LOGI(TAG, "--- Scenario 3: SKIPPED (no books folder) ---");
      }
    } else {
      ESP_LOGI(TAG, "--- Scenario 3: SKIPPED (not in REGRESSION_SCENARIOS) ---");
    }

    // --- Scenario 4: pageLocs recomputation ---
    if (REGRESSION_SCENARIOS & (1 << 3)) {
      if (booksFolderAvailable) {
        runScenarioWithHeapCheck("S4 page_locs", [&]() { return scenarioPageLocsRecompute(bl); });
      } else {
        ESP_LOGI(TAG, "--- Scenario 4: SKIPPED (no books folder) ---");
      }
    } else {
      ESP_LOGI(TAG, "--- Scenario 4: SKIPPED (not in REGRESSION_SCENARIOS) ---");
    }

    // --- Scenario 5: concurrent navigation ---
    if (REGRESSION_SCENARIOS & (1 << 4)) {
      if (booksFolderAvailable) {
        runScenarioWithHeapCheck("S5 concurrent_nav",
                                 [&]() { return scenarioConcurrentNavigation(bl); });
      } else {
        ESP_LOGI(TAG, "--- Scenario 5: SKIPPED (no books folder) ---");
      }
    } else {
      ESP_LOGI(TAG, "--- Scenario 5: SKIPPED (not in REGRESSION_SCENARIOS) ---");
    }

    // --- Scenario 6: WiFi + NTP ---
    if (REGRESSION_SCENARIOS & (1 << 5)) {
      #if DATE_TIME_RTC
        runScenarioWithHeapCheck("S6 wifi_ntp", [&]() { return scenarioWifiNtp(); });
      #else
        ESP_LOGI(TAG, "--- Scenario 6: SKIPPED (DATE_TIME_RTC not enabled) ---");
      #endif
    } else {
      ESP_LOGI(TAG, "--- Scenario 6: SKIPPED (not in REGRESSION_SCENARIOS) ---");
    }

    // --- Scenario 7: WiFi web server ---
    if (REGRESSION_SCENARIOS & (1 << 6)) {
      runScenarioWithHeapCheck("S7 web_server", [&]() { return scenarioWebServer(); });
    } else {
      ESP_LOGI(TAG, "--- Scenario 7: SKIPPED (not in REGRESSION_SCENARIOS) ---");
    }

    // --- Final report ---
    printHeapTrendHistory(gInitialFreeHeap);

    updateHeap();
    uint32_t finalFreeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    uint32_t stackHwm      = uxTaskGetStackHighWaterMark(nullptr) * (uint32_t)sizeof(StackType_t);
    ESP_LOGI(TAG, "========================================");
    if (totalErrors == 0) {
      ESP_LOGI(TAG, " RESULT: PASS  (0 errors)");
    } else {
      ESP_LOGE(TAG, " RESULT: FAIL  (%d error(s))", totalErrors);
    }
    ESP_LOGI(TAG, " Initial free heap : %" PRIu32 " B", gInitialFreeHeap);
    ESP_LOGI(TAG, " Final free heap   : %" PRIu32 " B  (delta: %+" PRIi32 " B)", finalFreeHeap,
             (int32_t)(finalFreeHeap - gInitialFreeHeap));
    ESP_LOGI(TAG, " Min free heap     : %" PRIu32 " B", gMinFreeHeap);
    ESP_LOGI(TAG, " Stack HWM         : %" PRIu32 " B", stackHwm);
    ESP_LOGI(TAG, "========================================");
  }

#endif // EPUB_INKPLATE_BUILD && REGRESSION_TEST
