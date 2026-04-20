// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// SimpleDB test suite
//
// Exercises SimpleDB end-to-end using a temporary file so that no
// pre-existing SDCard content is required.  The file is removed when the
// suite exits (pass or fail).
// ---------------------------------------------------------------------------

#include "simple_db.hpp"

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Logging / check helpers (same style as the himem and DOM suites).
// ---------------------------------------------------------------------------
static const char *const TAG = "simple_db_test";

#include <cstdio>
#define SDB_LOG(fmt, ...) std::printf("[simple_db_test] " fmt "\n", ##__VA_ARGS__)

static int sPass = 0;
static int sFail = 0;

#define SDB_CHECK(cond, msg)                                                                      \
  do {                                                                                            \
    if (!(cond)) {                                                                                \
      SDB_LOG("FAIL [%s:%d] " msg, __FILE__, __LINE__);                                           \
      ++sFail;                                                                                    \
    } else {                                                                                      \
      SDB_LOG("PASS " msg);                                                                       \
      ++sPass;                                                                                    \
    }                                                                                             \
  } while (0)

// ---------------------------------------------------------------------------
// A trivial plain-old-data record used as test payload.
// ---------------------------------------------------------------------------
struct TestRecord {
  int32_t  id;
  char     name[32];
  int32_t  value;
};

static const char *const TMP_DB = "/tmp/epub_test_simpledb.db";

// ---------------------------------------------------------------------------
// 1. Create / open lifecycle
// ---------------------------------------------------------------------------
static void testLifecycle() {
  SDB_LOG("--- lifecycle: create / open / close ---");

  remove(TMP_DB); // ensure a fresh start

  auto db = SimpleDB::Make();
  SDB_CHECK(db != nullptr, "Make() returns non-null");

  // create() via open() when file does not exist
  SDB_CHECK(db->open(TMP_DB),   "open() on non-existing file creates it");
  SDB_CHECK(db->isDbOpen(),     "isDbOpen() true after open");
  SDB_CHECK(db->getRecordCount() == 0, "new database has zero records");

  db->close();
  SDB_CHECK(!db->isDbOpen(), "isDbOpen() false after close");

  // Re-open the now-existing (empty) file
  SDB_CHECK(db->open(TMP_DB), "re-open existing file succeeds");
  SDB_CHECK(db->isDbOpen(),   "isDbOpen() true after re-open");
  SDB_CHECK(db->getRecordCount() == 0, "empty database still has zero records");

  db->close();
}

// ---------------------------------------------------------------------------
// 2. addRecord / getRecord round-trip
// ---------------------------------------------------------------------------
static void testAddGet() {
  SDB_LOG("--- addRecord / getRecord ---");

  remove(TMP_DB);

  auto db = SimpleDB::Make();
  db->open(TMP_DB);

  // Write three distinct records.
  TestRecord w1 = {1, "Alice",   100};
  TestRecord w2 = {2, "Bob",     200};
  TestRecord w3 = {3, "Charlie", 300};

  SDB_CHECK(db->addRecord(&w1, sizeof(w1)), "addRecord record 1 succeeds");
  SDB_CHECK(db->addRecord(&w2, sizeof(w2)), "addRecord record 2 succeeds");
  SDB_CHECK(db->addRecord(&w3, sizeof(w3)), "addRecord record 3 succeeds");
  SDB_CHECK(db->getRecordCount() == 3,      "getRecordCount() == 3 after 3 adds");

  // Read them back in order.
  db->setCurrentIdx(0);
  TestRecord r{};

  SDB_CHECK(db->getRecord(&r, sizeof(r)),            "getRecord at index 0 succeeds");
  SDB_CHECK(r.id == 1 && strcmp(r.name, "Alice") == 0 && r.value == 100,
            "record 0 content matches w1");

  db->setCurrentIdx(1);
  SDB_CHECK(db->getRecord(&r, sizeof(r)),            "getRecord at index 1 succeeds");
  SDB_CHECK(r.id == 2 && strcmp(r.name, "Bob") == 0 && r.value == 200,
            "record 1 content matches w2");

  db->setCurrentIdx(2);
  SDB_CHECK(db->getRecord(&r, sizeof(r)),            "getRecord at index 2 succeeds");
  SDB_CHECK(r.id == 3 && strcmp(r.name, "Charlie") == 0 && r.value == 300,
            "record 2 content matches w3");

  db->close();
}

// ---------------------------------------------------------------------------
// 3. getPartialRecord
// ---------------------------------------------------------------------------
static void testPartialRead() {
  SDB_LOG("--- getPartialRecord ---");

  remove(TMP_DB);

  auto db = SimpleDB::Make();
  db->open(TMP_DB);

  TestRecord w = {42, "Partial", 9999};
  db->addRecord(&w, sizeof(w));

  db->setCurrentIdx(0);

  // Read just the id field (first 4 bytes).
  int32_t idOnly = 0;
  SDB_CHECK(db->getPartialRecord(&idOnly, sizeof(idOnly), offsetof(TestRecord, id)),
            "getPartialRecord for id field succeeds");
  SDB_CHECK(idOnly == 42, "partial read returns correct id");

  // Read just the value field.
  int32_t valOnly = 0;
  SDB_CHECK(db->getPartialRecord(&valOnly, sizeof(valOnly), offsetof(TestRecord, value)),
            "getPartialRecord for value field succeeds");
  SDB_CHECK(valOnly == 9999, "partial read returns correct value");

  db->close();
}

// ---------------------------------------------------------------------------
// 4. getRecordSize
// ---------------------------------------------------------------------------
static void testRecordSize() {
  SDB_LOG("--- getRecordSize ---");

  remove(TMP_DB);

  auto db = SimpleDB::Make();
  db->open(TMP_DB);

  TestRecord w = {7, "SizeTest", 0};
  db->addRecord(&w, sizeof(w));

  db->setCurrentIdx(0);
  SDB_CHECK(db->getRecordSize() == static_cast<int32_t>(sizeof(TestRecord)),
            "getRecordSize() matches sizeof(TestRecord)");

  // Past-end returns 0.
  db->setCurrentIdx(1);
  SDB_CHECK(db->getRecordSize() == 0, "getRecordSize() == 0 past end of DB");

  db->close();
}

// ---------------------------------------------------------------------------
// 5. gotoFirst / gotoNext
// ---------------------------------------------------------------------------
static void testNavigation() {
  SDB_LOG("--- gotoFirst / gotoNext ---");

  remove(TMP_DB);

  auto db = SimpleDB::Make();
  db->open(TMP_DB);

  for (int i = 1; i <= 5; ++i) {
    TestRecord w = {i, "nav", i * 10};
    db->addRecord(&w, sizeof(w));
  }

  SDB_CHECK(db->gotoFirst(), "gotoFirst() returns true for non-empty DB");
  SDB_CHECK(db->getCurrentIdx() == 0, "gotoFirst() lands on index 0");

  int count = 1;
  while (db->gotoNext()) ++count;
  SDB_CHECK(count == 5, "gotoNext() iterates all 5 records");

  // Empty database.
  remove(TMP_DB);
  auto db2 = SimpleDB::Make();
  db2->open(TMP_DB);
  SDB_CHECK(!db2->gotoFirst(), "gotoFirst() returns false on empty DB");
  db2->close();

  db->close();
}

// ---------------------------------------------------------------------------
// 6. setDeleted / someRecordsWereDeleted
// ---------------------------------------------------------------------------
static void testDelete() {
  SDB_LOG("--- setDeleted / iteration skips deleted ---");

  remove(TMP_DB);

  auto db = SimpleDB::Make();
  db->open(TMP_DB);

  for (int i = 1; i <= 4; ++i) {
    TestRecord w = {i, "del", i};
    db->addRecord(&w, sizeof(w));
  }

  SDB_CHECK(!db->someRecordsWereDeleted(), "no deleted records initially");

  // Mark record at index 1 as deleted.
  db->setCurrentIdx(1);
  db->setDeleted();
  SDB_CHECK(db->someRecordsWereDeleted(), "someRecordsWereDeleted() true after setDeleted");

  // gotoFirst / gotoNext must skip deleted records.
  db->gotoFirst();
  SDB_CHECK(db->getCurrentIdx() == 0, "first valid record is index 0");

  db->gotoNext();
  SDB_CHECK(db->getCurrentIdx() == 2, "gotoNext() skips deleted index 1 → lands on 2");

  db->gotoNext();
  SDB_CHECK(db->getCurrentIdx() == 3, "gotoNext() lands on index 3");

  SDB_CHECK(!db->gotoNext(), "gotoNext() returns false after last valid record");

  db->close();
}

// ---------------------------------------------------------------------------
// 7. Persistence across close/reopen
// ---------------------------------------------------------------------------
static void testPersistence() {
  SDB_LOG("--- persistence across close / reopen ---");

  remove(TMP_DB);

  {
    auto db = SimpleDB::Make();
    db->open(TMP_DB);
    TestRecord w1 = {10, "Persist1", 111};
    TestRecord w2 = {20, "Persist2", 222};
    db->addRecord(&w1, sizeof(w1));
    db->addRecord(&w2, sizeof(w2));
    db->close();
  }

  auto db = SimpleDB::Make();
  SDB_CHECK(db->open(TMP_DB), "reopen persisted file succeeds");
  SDB_CHECK(db->getRecordCount() == 2, "record count preserved after reopen");

  db->setCurrentIdx(0);
  TestRecord r{};
  db->getRecord(&r, sizeof(r));
  SDB_CHECK(r.id == 10 && r.value == 111, "record 0 survives close/reopen");

  db->setCurrentIdx(1);
  db->getRecord(&r, sizeof(r));
  SDB_CHECK(r.id == 20 && r.value == 222, "record 1 survives close/reopen");

  db->close();
  remove(TMP_DB);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
auto testSimpleDb() -> bool {
  sPass = 0;
  sFail = 0;

  SDB_LOG("========== SimpleDB test suite start ==========");

  testLifecycle();
  testAddGet();
  testPartialRead();
  testRecordSize();
  testNavigation();
  testDelete();
  testPersistence();

  SDB_LOG("========== SimpleDB test suite end: %d passed, %d failed ==========", sPass, sFail);
  return sFail == 0;
}
