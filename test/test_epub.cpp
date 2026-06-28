// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test_epub.cpp — unit / integration tests for the EPub class
//
// Tests are grouped into the following sub-suites:
//
//   1. testOpenClose        — open() on a valid epub succeeds; closeFile() cleans up.
//   2. testMetadata         — getTitle(), getAuthor(), getDescription() return correct values.
//   3. testUniqueId         — getUniqueIdentifier() extracts the dc:identifier UUID.
//   4. testItemCount        — getItemCount() returns the spine item count (2 for minimal.epub).
//   5. testOpfBasePath      — opfBasePath is extracted correctly from container.xml.
//   6. testGetItemAtIndex   — getItemAtIndex(0) succeeds; XML document is populated.
//   7. testCoverFilename    — getCoverFilename() resolves the cover href from OPF metadata.
//   8. testBadMimetype      — open() returns false when mimetype content is wrong.
//   9. testNoContainer      — open() returns false when META-INF/container.xml is absent.
//  10. testNoMetadata       — open() succeeds but getTitle()/getAuthor() return empty string.
//  11. testReopenSameFile   — calling open() twice with the same path is a no-op (returns true).
//  12. testReopenOtherFile  — calling open() with a different path re-opens correctly.
//
// The test binary must be run from the repository root so that the relative
// path "test/fixtures/*.epub" resolves correctly.
//
// NOTE: epub.cpp transitively uses the global `config` (defined in
// test_app_config.cpp via __CONFIG__=1) and `fonts` (defined in stubs.cpp via
// __FONTS__=1).  Both are linked into the same test binary.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstring>

// epub.hpp includes viewers/page.hpp → fonts.hpp, models/css.hpp,
// models/display_list.hpp.  None of these drag in GTK.  The problematic
// msg_viewer.hpp include is in epub.cpp (not epub.hpp) and is shadowed by
// test/stubs/viewers/msg_viewer.hpp via -I test/stubs.
#include "models/epub.hpp"
#include "test_stats.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

#define EPUB_LOG(fmt, ...) std::printf("[epub_test] " fmt "\n", ##__VA_ARGS__)

static int sPass = 0;
static int sFail = 0;

#define EPUB_CHECK(cond, msg)                                                                      \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      EPUB_LOG("FAIL [%s:%d] " msg, __FILE__, __LINE__);                                           \
      ++sFail;                                                                                     \
    } else {                                                                                       \
      EPUB_LOG("PASS " msg);                                                                       \
      ++sPass;                                                                                     \
    }                                                                                              \
  } while (0)

static constexpr const char *MINIMAL      = "test/fixtures/minimal.epub";
static constexpr const char *BAD_MIME     = "test/fixtures/bad_mimetype.epub";
static constexpr const char *NO_CONTAINER = "test/fixtures/no_container.epub";
static constexpr const char *NO_METADATA  = "test/fixtures/no_metadata.epub";

// ---------------------------------------------------------------------------
// Helper: fresh EPub object opened on a file.  Returns nullptr on failure.
// ---------------------------------------------------------------------------
static auto makeEPub(const char *path) -> EPubPtr {
  auto epub = EPub::Make();
  if (!epub) return nullptr;
  epub->open(path);
  return epub;
}

// ---------------------------------------------------------------------------
// Sub-test 1 — open / close lifecycle
// ---------------------------------------------------------------------------
static auto testOpenClose() -> bool {
  EPUB_LOG("--- open / close lifecycle ---");

  auto epub = EPub::Make();
  EPUB_CHECK(epub != nullptr, "EPub::Make() returns non-null");

  bool opened = epub->open(MINIMAL);
  EPUB_CHECK(!epub->filenameIsEmpty(), "filename is non-empty after open");

  bool closed = epub->closeFile();
  EPUB_CHECK(closed, "closeFile() returns true");

  EPUB_CHECK(epub->filenameIsEmpty(), "filename is empty after close");
  EPUB_CHECK(epub->getItemCount() == 0, "getItemCount() == 0 after close");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 2 — metadata accessors
// ---------------------------------------------------------------------------
static auto testMetadata() -> bool {
  EPUB_LOG("--- metadata ---");

  auto epub = EPub::Make();
  epub->open(MINIMAL);

  const char *title  = epub->getTitle();
  const char *author = epub->getAuthor();
  const char *desc   = epub->getDescription();

  EPUB_CHECK(title != nullptr, "getTitle() not null");
  EPUB_CHECK(author != nullptr, "getAuthor() not null");
  EPUB_CHECK(desc != nullptr, "getDescription() not null");

  EPUB_CHECK(std::strcmp(title, "Test Book Title") == 0, "title matches");
  EPUB_CHECK(std::strcmp(author, "Test Author") == 0, "author matches");
  EPUB_CHECK(std::strcmp(desc, "A minimal test epub for unit tests.") == 0, "description matches");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 3 — unique identifier / encryption key state
// ---------------------------------------------------------------------------
static auto testUniqueId() -> bool {
  EPUB_LOG("--- unique identifier / key state ---");

  // For a plain (non-DRM) epub, encryption.xml is absent so getKeys() is never
  // called.  The binUuid array must remain all-zeros; encryptionIsPresent() must
  // be false.
  auto epub = EPub::Make();
  epub->open(MINIMAL);

  EPUB_CHECK(!epub->encryptionIsPresent(), "encryptionIsPresent() == false for plain epub");

  // binUuid should be zero-initialised (no encryption.xml → getKeys() not called).
  const auto &uuid = epub->getBinUuid();
  bool allZero     = true;
  for (size_t i = 0; i < sizeof(EPub::BinUUID); ++i) {
    if (uuid[i] != 0) {
      allZero = false;
      break;
    }
  }
  EPUB_CHECK(allZero, "binUuid all-zero for non-encrypted epub");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 4 — spine item count
// ---------------------------------------------------------------------------
static auto testItemCount() -> bool {
  EPUB_LOG("--- item count ---");

  auto epub = EPub::Make();
  epub->open(MINIMAL);

  int16_t count = epub->getItemCount();
  EPUB_CHECK(count == 2, "getItemCount() == 2 for minimal.epub (ch1 + ch2)");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 5 — OPF base path
// ---------------------------------------------------------------------------
static auto testOpfBasePath() -> bool {
  EPUB_LOG("--- OPF base path ---");

  auto epub = EPub::Make();
  epub->open(MINIMAL);

  // OPF is at OEBPS/content.opf → basePath should be "OEBPS/"
  const HimemString &base = epub->getOpfBasePath();
  EPUB_CHECK(base == "OEBPS/", "opfBasePath == \"OEBPS/\"");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 6 — getItemAtIndex
// ---------------------------------------------------------------------------
static auto testGetItemAtIndex() -> bool {
  EPUB_LOG("--- getItemAtIndex ---");

  auto epub = EPub::Make();
  epub->open(MINIMAL);

  bool ok0 = epub->getItemAtIndex(0);
  EPUB_CHECK(ok0, "getItemAtIndex(0) succeeds");

  const auto &item0 = epub->getCurrentItemInfo();
  EPUB_CHECK(!item0.xmlDoc.empty(), "item 0 XML document is non-empty");
  EPUB_CHECK(item0.itemrefIndex == 0, "itemrefIndex == 0");

  // Verify title element of ch1.xhtml
  auto titleNode = item0.xmlDoc.child("html").child("head").child("title");
  EPUB_CHECK(!titleNode.empty(), "html/head/title element present in ch1");
  EPUB_CHECK(std::strcmp(titleNode.child_value(), "Chapter One") == 0,
             "ch1 title text == \"Chapter One\"");

  bool ok1 = epub->getItemAtIndex(1);
  EPUB_CHECK(ok1, "getItemAtIndex(1) succeeds");
  EPUB_CHECK(epub->getItemrefIndex() == 1, "getItemrefIndex() == 1 after load");

  // Out-of-bounds index should fail gracefully.
  bool okOob = epub->getItemAtIndex(99);
  EPUB_CHECK(!okOob, "getItemAtIndex(99) returns false (out of range)");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 7 — cover filename
// ---------------------------------------------------------------------------
static auto testCoverFilename() -> bool {
  EPUB_LOG("--- cover filename ---");

  auto epub = EPub::Make();
  epub->open(MINIMAL);

  const char *cover = epub->getCoverFilename();
  EPUB_CHECK(cover != nullptr, "getCoverFilename() not null");
  EPUB_CHECK(cover[0] != '\0', "getCoverFilename() not empty");
  // The OPF maps cover-img → cover.jpg
  EPUB_CHECK(std::strcmp(cover, "cover.jpg") == 0, "cover filename == \"cover.jpg\"");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 8 — bad mimetype
// ---------------------------------------------------------------------------
static auto testBadMimetype() -> bool {
  EPUB_LOG("--- bad mimetype ---");

  auto epub   = EPub::Make();
  bool opened = epub->open(BAD_MIME);
  EPUB_CHECK(!opened, "open(bad_mimetype.epub) returns false");
  EPUB_CHECK(epub->filenameIsEmpty(), "filename empty after failed open");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 9 — missing container.xml
// ---------------------------------------------------------------------------
static auto testNoContainer() -> bool {
  EPUB_LOG("--- no META-INF/container.xml ---");

  auto epub   = EPub::Make();
  bool opened = epub->open(NO_CONTAINER);
  EPUB_CHECK(!opened, "open(no_container.epub) returns false");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 10 — epub with no Dublin Core metadata
// ---------------------------------------------------------------------------
static auto testNoMetadata() -> bool {
  EPUB_LOG("--- no dc:title / dc:creator metadata ---");

  auto epub   = EPub::Make();
  bool opened = epub->open(NO_METADATA);
  EPUB_CHECK(opened, "open(no_metadata.epub) returns true (structural epub is valid)");

  // getMeta() returns empty string (not nullptr) when the element is absent.
  const char *title  = epub->getTitle();
  const char *author = epub->getAuthor();

  EPUB_CHECK(title == nullptr || title[0] == '\0', "getTitle()  empty for no-metadata epub");
  EPUB_CHECK(author == nullptr || author[0] == '\0', "getAuthor() empty for no-metadata epub");

  int16_t count = epub->getItemCount();
  EPUB_CHECK(count == 1, "getItemCount() == 1 for no_metadata.epub");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 11 — re-opening the same file is a no-op
// ---------------------------------------------------------------------------
static auto testReopenSameFile() -> bool {
  EPUB_LOG("--- reopen same file ---");

  auto epub  = EPub::Make();
  bool first = epub->open(MINIMAL);
  EPUB_CHECK(first, "first open(minimal.epub) succeeds");

  bool second = epub->open(MINIMAL);
  EPUB_CHECK(second, "second open(same path) returns true");

  // File should still be readable after the second open call.
  const char *title = epub->getTitle();
  EPUB_CHECK(title != nullptr && std::strcmp(title, "Test Book Title") == 0,
             "title still valid after reopen");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Sub-test 12 — re-opening a different file closes the first one first
// ---------------------------------------------------------------------------
static auto testReopenOtherFile() -> bool {
  EPUB_LOG("--- reopen different file ---");

  auto epub = EPub::Make();
  epub->open(MINIMAL);
  EPUB_CHECK(!epub->filenameIsEmpty(), "minimal.epub opened");

  // Open the no_metadata epub — should implicitly close minimal first.
  bool ok = epub->open(NO_METADATA);
  EPUB_CHECK(ok, "open(no_metadata.epub) after minimal succeeds");

  HimemString fname = epub->getCurrentFilename();
  EPUB_CHECK(fname.find("no_metadata") != std::string::npos,
             "getCurrentFilename() points to second epub");

  return sFail == 0;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
auto testEPub() -> TestStats {
  int failsBefore;

  EPUB_LOG("========== EPub test suite start ==========");

  // Each sub-test captures only the NEW failures introduced by that block.
  auto run = [&](const char *name, auto fn) {
    failsBefore = sFail;
    fn();
    if (sFail == failsBefore)
      EPUB_LOG("  >>> %s: OK", name);
    else
      EPUB_LOG("  >>> %s: FAILED (%d new failures)", name, sFail - failsBefore);
  };

  run("open/close", testOpenClose);
  run("metadata", testMetadata);
  run("unique-id", testUniqueId);
  run("item-count", testItemCount);
  run("opf-base-path", testOpfBasePath);
  run("get-item-at-index", testGetItemAtIndex);
  run("cover-filename", testCoverFilename);
  run("bad-mimetype", testBadMimetype);
  run("no-container", testNoContainer);
  run("no-metadata", testNoMetadata);
  run("reopen-same", testReopenSameFile);
  run("reopen-other", testReopenOtherFile);

  EPUB_LOG("========== EPub test suite end: %d passed, %d failed ==========", sPass, sFail);

  return TestStats{sPass, sFail};
}
