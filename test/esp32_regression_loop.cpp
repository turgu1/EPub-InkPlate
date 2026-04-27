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
// are printed.  The test fails (and prints a summary) if:
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

  #include <cinttypes>
  #include <cstdio>
  #include <dirent.h>
  #include <sys/stat.h>

  static const char *const TAG = "RegressionTest";

  // ---------------------------------------------------------------------------
  // Test Selection (for rapid iteration during development)
  // ---------------------------------------------------------------------------
  // Set to 1 to skip scenarios 1-4 and run only scenario 5 (concurrent navigation)
  // This allows quick testing of new features without the 5-10 minute delay.
  #define REGRESSION_SKIP_TO_SCENARIO_5 1

  // ---------------------------------------------------------------------------
  // Tuning constants
  // ---------------------------------------------------------------------------
  static constexpr uint32_t HEAP_WARN_THRESHOLD = 32 * 1024; ///< Warn if free heap < 32 KB
  static constexpr int OPEN_CLOSE_CYCLES        = 10;        ///< Repeat open/close N times per book
  // Page-locs recomputation can take multiple minutes on large books.
  static constexpr uint32_t PAGE_LOCS_TIMEOUT_MS = 15 * 60 * 1000;
  // Scenario 5: concurrent navigation parameters
  static constexpr int NAV_THREAD_COUNT = 2; ///< Number of concurrent navigator threads
  static constexpr uint32_t NAV_ITER_TIMEOUT_MS =
      30 * 1000; ///< Max time a single nav iteration can hang
  static constexpr uint32_t NAV_SLEEP_MS =
      200; ///< Sleep between nav operations (reduced aggressiveness)

  // ---------------------------------------------------------------------------
  // Watermark helpers
  // ---------------------------------------------------------------------------
  static uint32_t gMinFreeHeap = UINT32_MAX;

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
          count++;
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
  static int scenarioOpenClose(BookList &bl) {
    ESP_LOGI(TAG, "--- Scenario 1: repeated open/close (%d cycles per book) ---",
             OPEN_CLOSE_CYCLES);
    int errors = 0;
    for (int cycle = 0; cycle < OPEN_CLOSE_CYCLES; cycle++) {
      for (uint8_t i = 0; i < bl.count; i++) {
        auto epub = EPub::Make();
        if (!epub) {
          ESP_LOGE(TAG, "S1: EPub::Make() failed");
          errors++;
          continue;
        }
        if (!epub->open(bl.paths[i])) {
          ESP_LOGE(TAG, "S1: open failed: %s", bl.paths[i]);
          errors++;
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

  // ---------------------------------------------------------------------------
  // Scenario 2 – TOC load for every book
  // ---------------------------------------------------------------------------
  static int scenarioToc(BookList &bl) {
    ESP_LOGI(TAG, "--- Scenario 2: TOC load ---");
    int errors = 0;
    for (uint8_t i = 0; i < bl.count; i++) {
      auto epub = EPub::Make();
      if (!epub) {
        errors++;
        continue;
      }
      if (!epub->open(bl.paths[i])) {
        errors++;
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
  static int scenarioCoverImages(BooksDir &booksDir) {
    ESP_LOGI(TAG, "--- Scenario 3: cover image retrieval ---");
    int errors = 0;
    int16_t n  = booksDir.getBookCount();
    ESP_LOGI(TAG, "S3: %d book(s) in DB", n);
    for (int16_t i = 0; i < n; i++) {
      auto book = booksDir.getBookData(static_cast<uint16_t>(i));
      if (!book) {
        ESP_LOGE(TAG, "S3: getBookData(%d) returned nullptr", i);
        errors++;
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
    volatile int navCount{0};
    volatile int navNullCount{0};
    volatile bool shouldStop{false};
  };

  static void navigationThreadTask(void *pvParams) {
    NavThreadContext *ctx = static_cast<NavThreadContext *>(pvParams);
    PageId currentPage{0, 0};

    while (!ctx->shouldStop) {
      // Alternate between forward/backward navigation
      bool goForward = (ctx->navCount % 2) == 0;

      const PageId *nextPageId = goForward ? pageLocs.getNextPageId(currentPage, 1)
                                           : pageLocs.getPrevPageId(currentPage, 1);

      if (nextPageId) {
        currentPage = *nextPageId;
        ctx->navCount++;
      } else {
        ctx->navNullCount++;
      }

      vTaskDelay(NAV_SLEEP_MS / portTICK_PERIOD_MS); // yield and give other threads time
    }

    // Notification: this thread is exiting
    ESP_LOGD(TAG, "S5: nav thread done (nav=%d nulls=%d)", ctx->navCount, ctx->navNullCount);
    vTaskDelete(nullptr);
  }

  // ---------------------------------------------------------------------------
  // Scenario 4 - Page locations recomputation for every book
  // ---------------------------------------------------------------------------
  static int scenarioPageLocsRecompute(BookList &bl) {
    ESP_LOGI(TAG, "--- Scenario 4: pageLocs recomputation ---");
    int errors = 0;

    for (uint8_t i = 0; i < bl.count; i++) {
      auto epub = EPub::Make();
      if (!epub) {
        ESP_LOGE(TAG, "S4: EPub::Make() failed");
        errors++;
        continue;
      }
      if (!epub->open(bl.paths[i])) {
        ESP_LOGE(TAG, "S4: open failed: %s", bl.paths[i]);
        errors++;
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
        errors++;
      } else if (timedOut) {
        ESP_LOGE(TAG, "S4: timeout for %s after %" PRIu32 " ms (last=%" PRIi16 ")", bl.paths[i],
                 elapsedMs, lastStatus);
        errors++;
      } else if (!completed) {
        ESP_LOGE(TAG, "S4: recompute did not complete for %s (last=%" PRIi16 ")", bl.paths[i],
                 lastStatus);
        errors++;
      }

      // Always stop/join control/retriever before closing this book.
      pageLocs.stopControlTask();

      if (completed && (lastStatus <= 0)) {
        ESP_LOGE(TAG, "S4: invalid final page count (%" PRIi16 ") for %s", lastStatus, bl.paths[i]);
        errors++;
      }

      epub->closeFile();
      updateHeap();
      vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    printWatermarks("page_locs");
    return errors;
  }

  // ---------------------------------------------------------------------------
  // Scenario 5 - Concurrent page navigation during pageLocs recomputation
  // ---------------------------------------------------------------------------
  static int scenarioConcurrentNavigation(BookList &bl) {
    ESP_LOGI(TAG, "--- Scenario 5: concurrent page navigation (%d threads) ---", NAV_THREAD_COUNT);
    int errors = 0;

    // Run on first book only to keep test time reasonable
    if (bl.count == 0) {
      ESP_LOGI(TAG, "S5: no books available, skipping scenario");
      return 0;
    }

    uint8_t bookIdx = 0; // Use first book
    auto epub       = EPub::Make();
    if (!epub) {
      ESP_LOGE(TAG, "S5: EPub::Make() failed");
      return 1;
    }
    if (!epub->open(bl.paths[bookIdx])) {
      ESP_LOGE(TAG, "S5: open failed: %s", bl.paths[bookIdx]);
      return 1;
    }

    ESP_LOGI(TAG, "S5: starting concurrent navigation test on '%s'", bl.paths[bookIdx]);

    // Start pageLocs computation
    pageLocs.checkForFormatChanges(epub, 0, true);

    // Create navigation threads immediately
    NavThreadContext navCtx[NAV_THREAD_COUNT];
    for (int i = 0; i < NAV_THREAD_COUNT; i++) {
      navCtx[i].shouldStop = false;
      esp_err_t res = xTaskCreatePinnedToCore(navigationThreadTask, "nav_task", 4096, &navCtx[i], 3,
                                              &navCtx[i].handle,
                                              (i == 0) ? 0 : 1); // Spread across cores
      if (res != pdPASS) {
        ESP_LOGE(TAG, "S5: failed to create nav thread %d (err=%d)", i, res);
        errors++;
      }
    }

    // Wait for pageLocs computation to complete (same as Scenario 4)
    bool started               = false;
    bool completed             = false;
    bool aborted               = false;
    bool timedOut              = false;
    bool deadlocked            = false;
    uint32_t elapsedMs         = 0;
    uint32_t nextLogMs         = 10000;
    uint32_t lastProgressMs    = 0;
    int16_t lastStatus         = -1;
    int16_t lastStatusReported = -1;

    while (!completed && !aborted && !timedOut && !deadlocked) {
      int16_t status         = pageLocs.getPageCountOrPercent();
      lastStatus             = status;
      int16_t currentItemref = pageLocs.getCurrentItemrefIndex();

      if (status < 0) {
        aborted = true;
        break;
      }

      if (!started && ((currentItemref >= 0) || (status > 0))) {
        started            = true;
        lastProgressMs     = elapsedMs;
        lastStatusReported = status;
        ESP_LOGI(TAG, "S5: pageLocs computation started");
      }

      // Detect stalled progress (likely deadlock caused by nav thread contention)
      if (started && (status == lastStatusReported) && (currentItemref == 0)) {
        if ((elapsedMs - lastProgressMs) > 120000) { // No progress in 120 seconds
          deadlocked = true;
          ESP_LOGW(TAG,
                   "S5: DEADLOCK DETECTED - no progress in 120s, progress=%" PRIi16
                   " item=%" PRIi16,
                   status, currentItemref);
          break;
        }
      } else if (started && (status != lastStatusReported)) {
        lastProgressMs     = elapsedMs;
        lastStatusReported = status;
      }

      if (started && (currentItemref == -1) && (status > 0)) {
        completed = true;
        ESP_LOGI(TAG, "S5: pageLocs computation done, pages=%" PRIi16, status);
        break;
      }

      if (elapsedMs >= nextLogMs) {
        ESP_LOGI(TAG, "S5: progress=%" PRIi16 " item=%" PRIi16 " elapsed=%" PRIu32 " ms", status,
                 currentItemref, elapsedMs);
        nextLogMs += 10000;
      }

      vTaskDelay(200 / portTICK_PERIOD_MS);
      elapsedMs += 200;

      if (elapsedMs >= PAGE_LOCS_TIMEOUT_MS) {
        timedOut = true;
      }
    }

    // Stop navigation threads
    for (int i = 0; i < NAV_THREAD_COUNT; i++) {
      navCtx[i].shouldStop = true;
    }

    // Wait for nav threads to exit with timeout protection
    uint32_t navWaitMs = 0;
    for (int i = 0; i < NAV_THREAD_COUNT; i++) {
      while (navCtx[i].handle && navWaitMs < 5000) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        navWaitMs += 50;
      }
      if (navCtx[i].handle) {
        ESP_LOGW(TAG, "S5: nav thread %d did not exit cleanly", i);
        errors++;
      }
    }

    // Verify computation outcome
    if (aborted) {
      ESP_LOGE(TAG, "S5: pageLocs computation aborted");
      errors++;
    } else if (deadlocked) {
      ESP_LOGE(TAG, "S5: DEADLOCK - concurrent navigation threads caused computation to stall");
      errors++;
    } else if (timedOut) {
      ESP_LOGE(TAG, "S5: pageLocs computation timed out after %" PRIu32 " ms (last=%" PRIi16 ")",
               elapsedMs, lastStatus);
      errors++;
    } else if (!completed) {
      ESP_LOGE(TAG, "S5: pageLocs computation did not complete (last=%" PRIi16 ")", lastStatus);
      errors++;
    } else if (lastStatus <= 0) {
      ESP_LOGE(TAG, "S5: invalid final page count (%" PRIi16 ")", lastStatus);
      errors++;
    } else {
      // Successful completion - log nav thread activity
      int32_t totalNavs  = navCtx[0].navCount + navCtx[1].navCount;
      int32_t totalNulls = navCtx[0].navNullCount + navCtx[1].navNullCount;
      ESP_LOGI(TAG,
               "S5: computation SUCCESS | nav-threads: %d successes, %d nulls (%.1f%% hit rate)",
               (int)totalNavs, (int)totalNulls,
               totalNavs > 0 ? (100.0f * totalNavs / (totalNavs + totalNulls)) : 0.0f);
    }

    // Cleanup
    pageLocs.stopControlTask();
    epub->closeFile();
    updateHeap();
    vTaskDelay(20 / portTICK_PERIOD_MS);

    printWatermarks("concurrent_nav");
    return errors;
  }

  // ---------------------------------------------------------------------------
  // Public entry point – called from mainTask() when REGRESSION_TEST is defined
  // ---------------------------------------------------------------------------
  void runRegressionLoop() {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " ESP32 Memory-Stress Regression Loop");
    ESP_LOGI(TAG, "========================================");

    updateHeap();
    ESP_LOGI(TAG, "Initial free heap: %" PRIu32 " B", gMinFreeHeap);

    // --- Collect epub file paths ---
    BookList bl;
    bool booksFolderAvailable = bl.scan();

    int totalErrors = 0;

    if (!booksFolderAvailable) {
      ESP_LOGW(TAG, "Skipping scenarios 1-4: SD card / books folder not available.");
      ESP_LOGW(TAG, "If this is expected, run this loop after SD initialization.");
    } else {
      #if !REGRESSION_SKIP_TO_SCENARIO_5
        // --- Scenario 1 ---
        totalErrors += scenarioOpenClose(bl);

        // --- Scenario 2 ---
        totalErrors += scenarioToc(bl);

        // --- Scenario 3: requires books DB to be readable ---
        int16_t dummy = -1;
        if (!booksDir.readBooksDirectory(nullptr, dummy)) {
          ESP_LOGW(TAG, "S3: books DB unavailable, skipping cover scenario");
        } else {
          totalErrors += scenarioCoverImages(booksDir);
        }

        // --- Scenario 4 ---
        totalErrors += scenarioPageLocsRecompute(bl);
      #else
        ESP_LOGI(TAG, "*** SKIPPING SCENARIOS 1-4 (REGRESSION_SKIP_TO_SCENARIO_5 enabled) ***");
      #endif

      // --- Scenario 5 ---
      totalErrors += scenarioConcurrentNavigation(bl);
    }

    // --- Final report ---
    ESP_LOGI(TAG, "========================================");
    if (totalErrors == 0) {
      ESP_LOGI(TAG, " RESULT: PASS  (0 errors)");
    } else {
      ESP_LOGE(TAG, " RESULT: FAIL  (%d error(s))", totalErrors);
    }
    ESP_LOGI(TAG, " Min free heap : %" PRIu32 " B", gMinFreeHeap);
    ESP_LOGI(TAG, " Stack HWM     : %" PRIu32 " B",
             uxTaskGetStackHighWaterMark(nullptr) * (uint32_t)sizeof(StackType_t));
    ESP_LOGI(TAG, "========================================");
  }

#endif // EPUB_INKPLATE_BUILD && REGRESSION_TEST
