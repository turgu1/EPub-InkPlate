// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test_unzip.cpp - unit / integration tests for Unzip
//
// Uses the EPUB fixture generated under test/fixtures/minimal.epub.
// ---------------------------------------------------------------------------

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

#include "test_stats.hpp"
#include "unzip.hpp"

#define UNZIP_LOG(fmt, ...) std::printf("[unzip_test] " fmt "\n", ##__VA_ARGS__)

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
  if (!opened) return false;

  UNZIP_CHECK(unzip.fileExists("mimetype"), "mimetype exists");

  int32_t size = unzip.getFileSize("mimetype");
  UNZIP_CHECK(size > 0, "getFileSize(mimetype) > 0");

  uint32_t outSize = 0;
  auto data        = unzip.getFile("mimetype", outSize);

  UNZIP_CHECK(data != nullptr, "getFile(mimetype) returns data");
  UNZIP_CHECK(outSize > 0, "getFile(mimetype) returns non-zero size");

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
  if (!opened) return false;

  const char *opfPath = "OEBPS/content.opf";
  UNZIP_CHECK(unzip.fileExists(opfPath), "content.opf exists");

  uint32_t outSize = 0;
  auto data        = unzip.getFile(opfPath, outSize);

  UNZIP_CHECK(data != nullptr, "getFile(content.opf) returns data");
  UNZIP_CHECK(outSize > 0, "getFile(content.opf) returns non-zero size");

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
  if (!opened) return false;

  uint32_t fileSize = 0;
  bool streamOpened = unzip.openStreamFile("mimetype", fileSize);
  UNZIP_CHECK(streamOpened, "openStreamFile(mimetype) succeeds");
  if (!streamOpened) {
    unzip.closeZipFile();
    return false;
  }

  // This used to be a potential self-deadlock path with a non-recursive mutex.
  unzip.closeZipFile();

  UNZIP_CHECK(!unzip.fileExists("mimetype"), "zip is closed after closeZipFile during stream");

  return sFail == 0;
}

static auto testCloseStreamFromNonOwnerThread() -> bool {
  UNZIP_LOG("--- close stream from non-owner thread ---");

  bool opened = unzip.openZipFile(MINIMAL);
  UNZIP_CHECK(opened, "openZipFile(minimal.epub) succeeds");
  if (!opened) return false;

  uint32_t fileSize = 0;
  bool streamOpened = unzip.openStreamFile("mimetype", fileSize);
  UNZIP_CHECK(streamOpened, "openStreamFile(mimetype) succeeds");
  if (!streamOpened) {
    unzip.closeZipFile();
    return false;
  }

  std::atomic<bool> workerDone{false};
  std::thread worker([&]() {
    unzip.closeStreamFile();
    workerDone.store(true, std::memory_order_release);
  });
  worker.join();

  UNZIP_CHECK(workerDone.load(std::memory_order_acquire),
              "non-owner closeStreamFile returns without blocking");

  char data[8]{};
  uint32_t got = unzip.getStreamData(data, sizeof(data));
  UNZIP_CHECK(got > 0, "owner thread can still read stream after non-owner close attempt");

  unzip.closeStreamFile();
  unzip.closeZipFile();

  return sFail == 0;
}

static auto testGetStreamDataFromNonOwnerThread() -> bool {
  UNZIP_LOG("--- getStreamData from non-owner thread ---");

  bool opened = unzip.openZipFile(MINIMAL);
  UNZIP_CHECK(opened, "openZipFile(minimal.epub) succeeds");
  if (!opened) return false;

  uint32_t fileSize = 0;
  bool streamOpened = unzip.openStreamFile("mimetype", fileSize);
  UNZIP_CHECK(streamOpened, "openStreamFile(mimetype) succeeds");
  if (!streamOpened) {
    unzip.closeZipFile();
    return false;
  }

  std::atomic<uint32_t> workerRead{1};
  std::thread worker([&]() {
    char data[8]{};
    workerRead.store(unzip.getStreamData(data, sizeof(data)), std::memory_order_release);
  });
  worker.join();

  UNZIP_CHECK(workerRead.load(std::memory_order_acquire) == 0,
              "non-owner getStreamData is rejected");

  unzip.closeStreamFile();
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
    if (sFail == failsBefore)
      UNZIP_LOG("  >>> %s: OK", name);
    else
      UNZIP_LOG("  >>> %s: FAILED (%d new failures)", name, sFail - failsBefore);
  };

  run("open-close", testOpenClose);
  run("lookup-stored", testLookupAndReadStoredFile);
  run("read-deflated", testReadDeflatedFile);

#if !STB
  run("close-zip-during-stream", testCloseZipWhileStreamOpenSameThread);
  run("close-stream-non-owner", testCloseStreamFromNonOwnerThread);
  run("read-stream-non-owner", testGetStreamDataFromNonOwnerThread);
#endif

  UNZIP_LOG("========== Unzip test suite end: %d passed, %d failed ==========", sPass, sFail);

  // Keep global singleton in a clean state for following suites.
  unzip.closeZipFile();

  return TestStats{sPass, sFail};
}
