// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test_unzip.cpp - unit / integration tests for Unzip (Updated for C++23 RAII)
//
// Uses the EPUB fixture generated under test/fixtures/minimal.epub.
// ---------------------------------------------------------------------------

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

#include "test_stats.hpp"
#include "unzip.hpp"

#define UNZIP_LOG(fmt, ...) std::printf("[unzip_test] " fmt "\n", ## __VA_ARGS__)

static int sPass = 0;
static int sFail = 0;

#define UNZIP_CHECK(cond, msg)                                                                     \
        do {                                                                                             \
          if (!(cond)) {                                                                                 \
            UNZIP_LOG("FAIL [%s:%d] " msg, __FILE__, __LINE__);                                          \
            ++sFail;                                                                                     \
          } else {                                                                                       \
            UNZIP_LOG("PASS " msg);                                                                      \
            ++sPass;                                                                                     \
          }                                                                                              \
        } while (0)

static constexpr const char *MINIMAL = "test/fixtures/minimal.epub";

static auto testOpenClose() -> bool {
  UNZIP_LOG("--- open / close ---");

  bool opened = unzip.openZipFile(MINIMAL);
  UNZIP_CHECK(opened, "openZipFile(minimal.epub) returns true");

  unzip.closeZipFile();
  UNZIP_CHECK(!unzip.fileExists("mimetype"), "fileExists() is false after close");

  return sFail == 0;
}

static auto testLookupAndReadStoredFile() -> bool {
  UNZIP_LOG("--- lookup / read stored file ---");

  bool opened = unzip.openZipFile(MINIMAL);
  UNZIP_CHECK(opened, "openZipFile(minimal.epub) succeeds");
  if (!opened) { return false; }

  UNZIP_CHECK(unzip.fileExists("mimetype"), "mimetype exists");

  int32_t size = unzip.getFileSize("mimetype");
  UNZIP_CHECK(size > 0, "getFileSize(mimetype) > 0");

  uint32_t outSize = 0;
  auto     data        = unzip.getFile("mimetype", outSize);

  UNZIP_CHECK(data != nullptr, "getFile(mimetype) returns data");
  UNZIP_CHECK(outSize > 0,     "getFile(mimetype) returns non-zero size");

  if ((data != nullptr) && (outSize > 0)) {
    UNZIP_CHECK(std::strcmp((const char *)data.get(), "application/epub+zip") == 0,
                "mimetype payload matches application/epub+zip");
  }

  unzip.closeZipFile();
  return sFail == 0;
}

static auto testReadDeflatedFile() -> bool {
  UNZIP_LOG("--- read deflated content.opf ---");

  bool opened = unzip.openZipFile(MINIMAL);
  UNZIP_CHECK(opened, "openZipFile(minimal.epub) succeeds");
  if (!opened) { return false; }

  const char *opfPath = "OEBPS/content.opf";
  UNZIP_CHECK(unzip.fileExists(opfPath), "content.opf exists");

  uint32_t outSize = 0;
  auto     data        = unzip.getFile(opfPath, outSize);

  UNZIP_CHECK(data != nullptr, "getFile(content.opf) returns data");
  UNZIP_CHECK(outSize > 0,     "getFile(content.opf) returns non-zero size");

  if ((data != nullptr) && (outSize > 0)) {
    const char *txt = (const char *)data.get();
    UNZIP_CHECK(std::strstr(txt, "<package") != nullptr, "content.opf payload contains <package");
  }

  unzip.closeZipFile();
  return sFail == 0;
}

#if !STB

  static auto testCloseZipWhileStreamOpenSameThread() -> bool {
    UNZIP_LOG("--- close zip while stream open (same thread) ---");

    bool opened = unzip.openZipFile(MINIMAL);
    UNZIP_CHECK(opened, "openZipFile(minimal.epub) succeeds");
    if (!opened) { return false; }

    uint32_t fileSize = 0;
    bool     streamOpened = unzip.openStreamFile("mimetype", fileSize);
    UNZIP_CHECK(streamOpened, "openStreamFile(mimetype) succeeds");
    if (!streamOpened) {
      unzip.closeZipFile();
      return false;
    }

    // This passes safely now with recursive_mutex and automatic session resets!
    unzip.closeZipFile();

    UNZIP_CHECK(!unzip.fileExists("mimetype"), "zip is closed cleanly after closeZipFile during stream");

    return sFail == 0;
  }

  static auto testConcurrentStreamAccessContention() -> bool {
    UNZIP_LOG("--- concurrent stream contention and safe sequencing ---");

    bool opened = unzip.openZipFile(MINIMAL);
    UNZIP_CHECK(opened, "openZipFile(minimal.epub) succeeds");
    if (!opened) { return false; }

    // 1. Main thread locks and opens a stream session
    uint32_t fileSize = 0;
    bool     streamOpened = unzip.openStreamFile("mimetype", fileSize);
    UNZIP_CHECK(streamOpened, "openStreamFile(mimetype) succeeds");
    if (!streamOpened) {
      unzip.closeZipFile();
      return false;
    }

    std::atomic<bool> releaseWorker{ false };
    std::atomic<bool> workerThreadCompleted{ false };

    // 2. Launch worker thread, but keep it tightly controlled
    std::thread worker([&]() {
                       // Wait until the main thread explicitly allows the worker to attempt a lock
                       while (!releaseWorker.load(std::memory_order_acquire)) {
                         std::this_thread::yield();
                       }

                       uint32_t secondarySize = 0;
                       auto secondaryData = unzip.getFile("mimetype", secondarySize);

                       UNZIP_CHECK(secondaryData != nullptr, "Secondary thread safely recovers file after lock releases");
                       workerThreadCompleted.store(true, std::memory_order_release);
    });

    // 3. Main thread reads its stream data safely without any thread contention
    char     data{};
    uint32_t got = unzip.getStreamData(&data, sizeof(data));
    UNZIP_CHECK(got > 0, "Primary thread successfully reads data");

    // 4. Main thread closes stream, unlocking the recursive mutex completely
    unzip.closeStreamFile();

    // 5. Now that the lock is free, unleash the worker thread!
    releaseWorker.store(true, std::memory_order_release);

    // 6. Join and verify sequencing
    worker.join();
    UNZIP_CHECK(workerThreadCompleted.load(std::memory_order_acquire), "Worker completes cleanly after primary stream closing");

    unzip.closeZipFile();
    return sFail == 0;
  }


#endif

auto testUnzip() -> TestStats {
  int failsBefore;

  UNZIP_LOG("========== Unzip test suite start ==========");

  auto run = [&](const char *name, auto fn) {
               failsBefore = sFail;
               fn();
               if (sFail == failsBefore) {
                 UNZIP_LOG("  >>> %s: OK", name);
               }
               else {
                 UNZIP_LOG("  >>> %s: FAILED (%d new failures)", name, sFail - failsBefore);
               }
             };

    run("open-close",    testOpenClose);
    run("lookup-stored", testLookupAndReadStoredFile);
    run("read-deflated", testReadDeflatedFile);

  #if !STB
    run("close-zip-during-stream",      testCloseZipWhileStreamOpenSameThread);
    run("concurrent-stream-contention", testConcurrentStreamAccessContention);
  #endif

  UNZIP_LOG("========== Unzip test suite end: %d passed, %d failed ==========", sPass, sFail);

  unzip.closeZipFile();
  return TestStats{ sPass, sFail };
}
