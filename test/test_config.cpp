// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// ConfigBase<> test suite
//
// Rather than exercising the application-level Config singleton (which pulls
// in the full controller and viewer subsystem), this suite instantiates
// ConfigBase with a minimal, self-contained configuration definition.
//
// The temporary config file is written to /tmp and removed on exit.
// ---------------------------------------------------------------------------

#include "config_template.hpp"
#include "test_stats.hpp"

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Logging / check helpers
// ---------------------------------------------------------------------------
#define CFG_LOG(fmt, ...) std::printf("[config_test] " fmt "\n", ##__VA_ARGS__)

static int sPass = 0;
static int sFail = 0;

#define CFG_CHECK(cond, msg)                                                                       \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      CFG_LOG("FAIL [%s:%d] " msg, __FILE__, __LINE__);                                            \
      ++sFail;                                                                                     \
    } else {                                                                                       \
      CFG_LOG("PASS " msg);                                                                        \
      ++sPass;                                                                                     \
    }                                                                                              \
  } while (0)

// ---------------------------------------------------------------------------
// Minimal config definition — independent of the application Config type.
// ---------------------------------------------------------------------------

enum class TstId {
  LABEL,     ///< STRING
      COUNT, ///< INT (int32_t)
      RATIO, ///< INT64 (int64_t)
      FLAG,  ///< BYTE  (int8_t)
};

// Storage for live values.
static char tstLabel[64];
static int32_t tstCount;
static int64_t tstRatio;
static int8_t tstFlag;

// Storage for default values.
static const char defaultLabel[]  = "default-label";
static const int32_t defaultCount = 7;
static const int64_t defaultRatio = 123456789LL;
static const int8_t defaultFlag   = 1;

using TstConfig = ConfigBase<TstId, 4>;

// Static config descriptor array — must be defined exactly once.
// The explicit template specialisation avoids ODR issues.
template <>
TstConfig::CfgType TstConfig::cfg = {{
    {TstId::LABEL, TstConfig::EntryType::STRING, "label", tstLabel, defaultLabel, sizeof(tstLabel)},
    {TstId::COUNT, TstConfig::EntryType::INT, "count", &tstCount, &defaultCount, 0},
    {TstId::RATIO, TstConfig::EntryType::INT64, "ratio", &tstRatio, &defaultRatio, 0},
    {TstId::FLAG, TstConfig::EntryType::BYTE, "flag", &tstFlag, &defaultFlag, 0},
}};

static const char *const TMP_CFG = "/tmp/epub_test_config.txt";

// ---------------------------------------------------------------------------
// 1. Default-value initialisation (read() on a missing file triggers save)
// ---------------------------------------------------------------------------
static void testDefaults() {
  CFG_LOG("--- defaults ---");

  remove(TMP_CFG);

  TstConfig cfg(TMP_CFG, false);
  // read() will not find the file and call save(true), writing defaults.
  bool ok = cfg.read();
  CFG_CHECK(ok, "read() on missing file succeeds (creates defaults file)");

  // Values must now equal their defaults.
  std::string label;
  int32_t count = 0;
  int64_t ratio = 0;
  int8_t flag   = 0;

  CFG_CHECK(cfg.get(TstId::LABEL, label) && label == defaultLabel, "default label is correct");
  CFG_CHECK(cfg.get(TstId::COUNT, &count) && count == defaultCount, "default count is correct");
  CFG_CHECK(cfg.get(TstId::RATIO, &ratio) && ratio == defaultRatio, "default ratio is correct");
  CFG_CHECK(cfg.get(TstId::FLAG, &flag) && flag == defaultFlag, "default flag is correct");
}

// ---------------------------------------------------------------------------
// 2. put() / get() round-trip (in-memory, no file I/O)
// ---------------------------------------------------------------------------
static void testPutGet() {
  CFG_LOG("--- put / get round-trip ---");

  TstConfig cfg(TMP_CFG, false);
  cfg.read(); // seed defaults

  // Overwrite each field.
  std::string newLabel = "hello-world";
  cfg.put(TstId::LABEL, newLabel);
  cfg.put(TstId::COUNT, (int32_t)42);
  cfg.put(TstId::RATIO, (int64_t)987654321LL);
  cfg.put(TstId::FLAG, (int8_t)0);

  std::string label;
  int32_t count = 0;
  int64_t ratio = 0;
  int8_t flag   = 99; // sentinel

  CFG_CHECK(cfg.get(TstId::LABEL, label) && label == "hello-world", "put/get label round-trip");
  CFG_CHECK(cfg.get(TstId::COUNT, &count) && count == 42, "put/get count round-trip");
  CFG_CHECK(cfg.get(TstId::RATIO, &ratio) && ratio == 987654321LL, "put/get ratio round-trip");
  CFG_CHECK(cfg.get(TstId::FLAG, &flag) && flag == 0, "put/get flag round-trip");
  CFG_CHECK(cfg.isModified(), "isModified() true after put");
}

// ---------------------------------------------------------------------------
// 3. save() / read() persistence
// ---------------------------------------------------------------------------
static void testPersistence() {
  CFG_LOG("--- save / reload persistence ---");

  remove(TMP_CFG);

  // Write a known state.
  {
    TstConfig cfg(TMP_CFG, false);
    cfg.read(); // initialise to defaults first

    std::string lbl = "persisted";
    cfg.put(TstId::LABEL, lbl);
    cfg.put(TstId::COUNT, (int32_t)99);
    cfg.put(TstId::RATIO, (int64_t)-42LL);
    cfg.put(TstId::FLAG, (int8_t)3);
    CFG_CHECK(cfg.save(), "save() succeeds");
  }

  // Re-read into a fresh instance.
  TstConfig cfg2(TMP_CFG, false);
  CFG_CHECK(cfg2.read(), "read() of saved file succeeds");
  CFG_CHECK(!cfg2.isModified(), "isModified() false after clean read");

  std::string label;
  int32_t count = 0;
  int64_t ratio = 0;
  int8_t flag   = 0;

  CFG_CHECK(cfg2.get(TstId::LABEL, label) && label == "persisted",
            "reloaded label matches saved value");
  CFG_CHECK(cfg2.get(TstId::COUNT, &count) && count == 99, "reloaded count matches saved value");
  CFG_CHECK(cfg2.get(TstId::RATIO, &ratio) && ratio == -42LL, "reloaded ratio matches saved value");
  CFG_CHECK(cfg2.get(TstId::FLAG, &flag) && flag == 3, "reloaded flag matches saved value");

  remove(TMP_CFG);
}

// ---------------------------------------------------------------------------
// 4. save(force=false) does nothing when unmodified
// ---------------------------------------------------------------------------
static void testSaveOnlyWhenModified() {
  CFG_LOG("--- save(force=false) respects isModified ---");

  remove(TMP_CFG);

  TstConfig cfg(TMP_CFG, false);
  cfg.read(); // creates file with defaults

  // Touch the timestamp: remove and check save() without force does NOT recreate.
  remove(TMP_CFG);
  CFG_CHECK(cfg.save(false), "save(false) returns true even when unmodified");

  // File should NOT exist (nothing was modified).
  FILE *f = fopen(TMP_CFG, "r");
  CFG_CHECK(f == nullptr, "file is NOT written when nothing is modified");
  if (f) fclose(f);

  // Now modify and save.
  std::string lbl = "modified";
  cfg.put(TstId::LABEL, lbl);
  CFG_CHECK(cfg.save(false), "save(false) writes file after modification");
  f = fopen(TMP_CFG, "r");
  CFG_CHECK(f != nullptr, "file IS written after modification");
  if (f) fclose(f);

  remove(TMP_CFG);
}

// ---------------------------------------------------------------------------
// 5. Comments in config file are ignored on read
// ---------------------------------------------------------------------------
static void testCommentHandling() {
  CFG_LOG("--- comment handling ---");

  remove(TMP_CFG);

  // Write a hand-crafted config file with comments and blank lines.
  {
    FILE *f = fopen(TMP_CFG, "w");
    if (!f) {
      CFG_LOG("SKIP — cannot write temp file");
      return;
    }
    std::fprintf(f, "# This is a comment\n");
    std::fprintf(f, "\n");
    std::fprintf(f, "label = \"commented-label\"\n");
    std::fprintf(f, "# another comment\n");
    std::fprintf(f, "count = 77\n");
    std::fprintf(f, "ratio = 55\n");
    std::fprintf(f, "flag = 2\n");
    fclose(f);
  }

  TstConfig cfg(TMP_CFG, false);
  CFG_CHECK(cfg.read(), "read() with comments succeeds");

  std::string label;
  int32_t count = 0;

  CFG_CHECK(cfg.get(TstId::LABEL, label) && label == "commented-label",
            "label parsed correctly despite surrounding comments");
  CFG_CHECK(cfg.get(TstId::COUNT, &count) && count == 77,
            "count parsed correctly despite surrounding comments");

  remove(TMP_CFG);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
auto testConfig() -> TestStats {
  sPass = 0;
  sFail = 0;

  CFG_LOG("========== Config test suite start ==========");

  testDefaults();
  testPutGet();
  testPersistence();
  testSaveOnlyWhenModified();
  testCommentHandling();

  CFG_LOG("========== Config test suite end: %d passed, %d failed ==========", sPass, sFail);
  return TestStats{sPass, sFail};
}
