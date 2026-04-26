// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test_app_config.cpp — integration tests for the application Config singleton
//
// Two complementary test strategies:
//
//  A. ConfigBase<> template mechanics (using the full application Config type)
//     — default values, put/get round-trip, save/reload, comment skipping,
//       save-only-when-modified, quoted & unquoted string values.
//
//  B. Fixture-driven parsing: read test/fixtures/app_config.txt and verify
//     every field is decoded to its expected value (type STRING, INT, BYTE).
//
// The test binary must be run from the repository root so that the relative
// path "test/fixtures/app_config.txt" resolves correctly.
//
// NOTE: The application Config type is instantiated here by defining
// __CONFIG__ for this translation unit only.  That exposes the CfgType
// specialisation and the 'config' global that live inside config.hpp.
// ---------------------------------------------------------------------------

#define __GLOBAL__ 0 // inhibit global.hpp's own extern-storage block
#define __CONFIG__ 1 // trigger the config.hpp storage / specialisation block
#include "models/config.hpp"
#undef __CONFIG__

#include "test_stats.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Logging / check helpers
// ---------------------------------------------------------------------------
#define ACFG_LOG(fmt, ...) std::printf("[app_config_test] " fmt "\n", ##__VA_ARGS__)

static int sPass = 0;
static int sFail = 0;

#define ACFG_CHECK(cond, msg)                                                                      \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      ACFG_LOG("FAIL [%s:%d] " msg, __FILE__, __LINE__);                                           \
      ++sFail;                                                                                     \
    } else {                                                                                       \
      ACFG_LOG("PASS " msg);                                                                       \
      ++sPass;                                                                                     \
    }                                                                                              \
  } while (0)

// ---------------------------------------------------------------------------
// 1. Default values (read() on a missing file)
// ---------------------------------------------------------------------------
static void testAppDefaults() {
  ACFG_LOG("--- app defaults ---");

  const char *tmp = "/tmp/epub_app_cfg_defaults.txt";
  remove(tmp);

  Config c(tmp, false);
  ACFG_CHECK(c.read(), "read() on missing file returns true");

  // Verify a representative set of defaults from config.hpp
  int8_t battery     = 0;
  int8_t orientation = 99;
  int8_t fontSize    = 0;
  int32_t port       = 0;

  ACFG_CHECK(c.get(ConfigIdent::BATTERY, &battery) && battery == 2,
             "default battery == 2 (VOLTAGE)");
  ACFG_CHECK(c.get(ConfigIdent::ORIENTATION, &orientation) && orientation == 1,
             "default orientation == 1 (RIGHT)");
  ACFG_CHECK(c.get(ConfigIdent::FONT_SIZE, &fontSize) && fontSize == 12, "default font_size == 12");
  ACFG_CHECK(c.get(ConfigIdent::PORT, &port) && port == 80, "default http_port == 80");
  HimemString ssid;
  ACFG_CHECK(c.get(ConfigIdent::SSID, ssid) && ssid == "NONE", "default wifi_ssid == NONE");

  remove(tmp);
}

// ---------------------------------------------------------------------------
// 2. put() / get() round-trip for every supported type
// ---------------------------------------------------------------------------
static void testAppPutGet() {
  ACFG_LOG("--- app put/get round-trip ---");

  const char *tmp = "/tmp/epub_app_cfg_putget.txt";
  remove(tmp);

  Config c(tmp, false);
  c.read(); // initialise to defaults

  // STRING
  HimemString newSsid = "my-network";
  c.put(ConfigIdent::SSID, newSsid);
  HimemString gotSsid;
  ACFG_CHECK(c.get(ConfigIdent::SSID, gotSsid) && gotSsid == "my-network",
             "put/get STRING (wifi_ssid) round-trip");

  // INT (int32_t)
  c.put(ConfigIdent::PORT, (int32_t)9090);
  int32_t gotPort = 0;
  ACFG_CHECK(c.get(ConfigIdent::PORT, &gotPort) && gotPort == 9090,
             "put/get INT (http_port) round-trip");

  // BYTE (int8_t)
  c.put(ConfigIdent::BATTERY, (int8_t)0);
  int8_t gotBattery = 99;
  ACFG_CHECK(c.get(ConfigIdent::BATTERY, &gotBattery) && gotBattery == 0,
             "put/get BYTE (battery) round-trip");

  ACFG_CHECK(c.isModified(), "isModified() true after puts");

  remove(tmp);
}

// ---------------------------------------------------------------------------
// 3. save() / reload() persistence
// ---------------------------------------------------------------------------
static void testAppPersistence() {
  ACFG_LOG("--- app save / reload ---");

  const char *tmp = "/tmp/epub_app_cfg_persist.txt";
  remove(tmp);

  {
    Config c(tmp, false);
    c.read();

    HimemString ssid = "persisted-ssid";
    c.put(ConfigIdent::SSID, ssid);
    c.put(ConfigIdent::PORT, (int32_t)7777);
    c.put(ConfigIdent::TIMEOUT, (int8_t)5);
    ACFG_CHECK(c.save(), "save() returns true");
  }

  Config c2(tmp, false);
  ACFG_CHECK(c2.read(), "read() of saved file succeeds");
  ACFG_CHECK(!c2.isModified(), "isModified() false after clean read");

  HimemString ssid;
  int32_t port   = 0;
  int8_t timeout = 0;

  ACFG_CHECK(c2.get(ConfigIdent::SSID, ssid) && ssid == "persisted-ssid",
             "ssid persisted correctly");
  ACFG_CHECK(c2.get(ConfigIdent::PORT, &port) && port == 7777, "http_port persisted correctly");
  ACFG_CHECK(c2.get(ConfigIdent::TIMEOUT, &timeout) && timeout == 5, "timeout persisted correctly");

  remove(tmp);
}

// ---------------------------------------------------------------------------
// 4. save(force=false) honours isModified()
// ---------------------------------------------------------------------------
static void testAppSaveWhenModified() {
  ACFG_LOG("--- app save only when modified ---");

  const char *tmp = "/tmp/epub_app_cfg_modified.txt";
  remove(tmp);

  Config c(tmp, false);
  c.read(); // creates file with defaults

  remove(tmp); // delete so we can detect a re-write

  ACFG_CHECK(c.save(false), "save(false) returns true");
  FILE *f = fopen(tmp, "r");
  ACFG_CHECK(f == nullptr, "file NOT re-written when unmodified");
  if (f) fclose(f);

  HimemString pwd = "new-password";
  c.put(ConfigIdent::PWD, pwd);
  ACFG_CHECK(c.save(false), "save(false) returns true after modification");
  f = fopen(tmp, "r");
  ACFG_CHECK(f != nullptr, "file written after modification");
  if (f) fclose(f);

  remove(tmp);
}

// ---------------------------------------------------------------------------
// 5. Fixture-driven parsing: test/fixtures/app_config.txt
//    Every field is checked against its expected value.
// ---------------------------------------------------------------------------
static void testFixtureParsing() {
  ACFG_LOG("--- fixture-driven parsing ---");

  const char *fixture = "test/fixtures/app_config.txt";

  Config c(fixture, false);
  bool ok = c.read();
  ACFG_CHECK(ok, "read() of fixture file succeeds");
  if (!ok) return;

  // STRING fields
  HimemString ssid, pwd, dnsName, apSsid, apPwd;
  ACFG_CHECK(c.get(ConfigIdent::SSID, ssid) && ssid == "TestSSID", "ssid == TestSSID");
  ACFG_CHECK(c.get(ConfigIdent::PWD, pwd) && pwd == "TestPWD", "pwd == TestPWD");
  ACFG_CHECK(c.get(ConfigIdent::DNS_NAME, dnsName) && dnsName == "testplate",
             "dns_name == testplate");
  ACFG_CHECK(c.get(ConfigIdent::AP_SSID, apSsid) && apSsid == "testplate-ap",
             "ap_ssid == testplate-ap");
  ACFG_CHECK(c.get(ConfigIdent::AP_PWD, apPwd) && apPwd == "ap-secret", "ap_pwd == ap-secret");

  // INT (int32_t)
  int32_t port = 0;
  ACFG_CHECK(c.get(ConfigIdent::PORT, &port) && port == 8080, "http_port == 8080");

  // BYTE (int8_t) fields
  int8_t battery = 0, timeout = 0, fontSize = 0, defaultFont = 0, useFonts = 0, showPic = 0,
         orient = 0, res = 0, showHeap = 0, showTitle = 0, frontLight = 0, dirView = 0,
         coverSize = 0;

  ACFG_CHECK(c.get(ConfigIdent::BATTERY, &battery) && battery == 1, "battery == 1");
  ACFG_CHECK(c.get(ConfigIdent::TIMEOUT, &timeout) && timeout == 30, "timeout == 30");
  ACFG_CHECK(c.get(ConfigIdent::FONT_SIZE, &fontSize) && fontSize == 10, "font_size == 10");
  ACFG_CHECK(c.get(ConfigIdent::DEFAULT_FONT, &defaultFont) && defaultFont == 0,
             "default_font == 0");
  ACFG_CHECK(c.get(ConfigIdent::USE_FONTS_IN_BOOKS, &useFonts) && useFonts == 0,
             "use_fonts_in_books == 0");
  ACFG_CHECK(c.get(ConfigIdent::SHOW_PICTURES, &showPic) && showPic == 1, "show_images == 1");
  ACFG_CHECK(c.get(ConfigIdent::ORIENTATION, &orient) && orient == 0, "orientation == 0");
  ACFG_CHECK(c.get(ConfigIdent::PIXEL_RESOLUTION, &res) && res == 1, "resolution == 1");
  ACFG_CHECK(c.get(ConfigIdent::SHOW_HEAP, &showHeap) && showHeap == 1, "show_heap == 1");
  ACFG_CHECK(c.get(ConfigIdent::SHOW_TITLE, &showTitle) && showTitle == 0, "show_title == 0");
  ACFG_CHECK(c.get(ConfigIdent::FRONT_LIGHT, &frontLight) && frontLight == 32, "front_light == 32");
  ACFG_CHECK(c.get(ConfigIdent::DIR_VIEW, &dirView) && dirView == 1, "dir_view == 1");
  ACFG_CHECK(c.get(ConfigIdent::COVER_SIZE, &coverSize) && coverSize == 1, "cover_size == 1");
}

// ---------------------------------------------------------------------------
// 6. Comment-header written by save(…, true) round-trips cleanly
// ---------------------------------------------------------------------------
static void testCommentHeader() {
  ACFG_LOG("--- comment header round-trip ---");

  const char *tmp = "/tmp/epub_app_cfg_comment.txt";
  remove(tmp);

  {
    Config c(tmp, true); // comment=true → writes the # header block
    c.read();            // missing file → save(true) → written with comments
  }

  // Verify the file exists and can be re-read.
  Config c2(tmp, false);
  ACFG_CHECK(c2.read(), "file with comment header round-trips via read()");

  // Default values must survive the comment header.
  int32_t port = 0;
  ACFG_CHECK(c2.get(ConfigIdent::PORT, &port) && port == 80,
             "http_port default intact after comment-header round-trip");

  remove(tmp);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
auto testAppConfig() -> TestStats {
  sPass = 0;
  sFail = 0;

  ACFG_LOG("========== App Config test suite start ==========");

  testAppDefaults();
  testAppPutGet();
  testAppPersistence();
  testAppSaveWhenModified();
  testFixtureParsing();
  testCommentHeader();

  ACFG_LOG("========== App Config test suite end: %d passed, %d failed ==========", sPass, sFail);
  return TestStats{sPass, sFail};
}
