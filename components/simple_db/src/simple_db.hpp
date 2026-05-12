// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"
#include "himem.hpp"

#include <cstdio>
#include <mutex>

/**
 * @brief Very simple database tool
 *
 * A *very* simple, one table database tool. Each record is having a single size that can
 * be different from each other.
 *
 * The tool maintains the location of each record in an array in RAM. No
 * index beyond that. When a database file is open, the tool computes the
 * location of each record. All record in file are considered valid.
 *
 * A record can be marked as deleted using the setDeleted() method. This
 * will only mark it as deleted in memory. A new database needs to be
 * created and filled with valid records by the application to get rid of
 * marked as deleted records.
 *
 * No memory allocation is done in the tool. This is the responsability
 * of the calling application.
 *
 * Each record is preceeded with its size in file. Nothing else is kept in file
 * then the data associated with the record.
 *
 * (c) 2020, Guy Turcotte
 */

using SimpleDBPtr = HimemUniquePtr<class SimpleDB>;

class SimpleDB {
private:
  static constexpr char const *TAG = "SimpleDB";

  static const uint16_t MAX_RECORD_COUNT = 2000;

  FILE *dbFile{nullptr};
  mutable std::recursive_mutex mutex;

  bool dbIsOpen;
  bool someRecordsDeleted{false};
  int32_t recordOffset[MAX_RECORD_COUNT]{}; ///< record offset in file
  bool isDeleted[MAX_RECORD_COUNT]{};       ///< true if record is deleted
  uint16_t recordCount;
  uint32_t fileSize{0};
  uint16_t currentRecordIdx; ///< Index of current record in recordOffset

public:
  SimpleDB() : dbIsOpen(false), recordCount(0), currentRecordIdx(0) {};
  SimpleDB(const SimpleDB &)            = delete;
  SimpleDB &operator=(const SimpleDB &) = delete;
  ~SimpleDB() {
    if (dbIsOpen) fclose(dbFile);
  }

  static inline auto Make() { return makeUniqueHimem<SimpleDB>(); }

  /**
   * @brief Open an existing database file.
   *
   * Once opened, the record_offsets vector is builts, scanning
   * the file to recover each record size. If the db file doesn't exists,
   * it will be created.
   *
   * @param filename
   * @return true The file has been opened.
   * @return false
   */
  auto open(const HimemString &filename) -> bool;

  /**
   * @brief Create a new database.
   *
   * The current database is closed and a new empty file is created.
   * If a file already exists, it will be overided.
   *
   * @param filename
   * @return true File created and database pointing at it.
   * @return false File already exists and not overrided. Nothing done.
   */
  auto create(const HimemString &filename) -> bool;

  auto close() -> void;

  [[nodiscard]] inline auto getCurrentIdx() -> uint16_t { return currentRecordIdx; }
  inline auto setCurrentIdx(int16_t index) -> void { currentRecordIdx = index; }
  [[nodiscard]] inline auto getRecordCount() -> uint16_t { return recordCount; }
  [[nodiscard]] inline auto getFileSize() -> uint16_t { return fileSize; }
  [[nodiscard]] inline auto someRecordsWereDeleted() -> bool { return someRecordsDeleted; }
  [[nodiscard]] inline auto isDbOpen() -> bool { return dbIsOpen; }

  /**
   * @brief Add a record at the end of the file.
   *
   * Does not change current location.
   *
   * @param record
   * @param size
   * @return true Record has been added.
   * @return false Potential file access issue.
   */
  auto addRecord(void *record, int32_t size) -> bool;

  auto getRecord(void *record, int32_t size) -> bool;

  auto getPartialRecord(void *record, int32_t size, int32_t offset) -> bool;

  auto show() -> void;

  /**
   * @brief Get size of the current record.
   *
   * Returns 0 if at end of the database
   */
  auto getRecordSize() -> int32_t {
    if ((currentRecordIdx >= recordCount) || isDeleted[currentRecordIdx]) return 0;
    if (currentRecordIdx == (recordCount - 1)) {
      return fileSize - recordOffset[currentRecordIdx] - sizeof(int32_t);
    }
    return recordOffset[currentRecordIdx + 1] - recordOffset[currentRecordIdx] - sizeof(int32_t);
  }

  /**
   * @brief Set current record as deleted.
   *
   */
  auto setDeleted() -> void {
    isDeleted[currentRecordIdx] = true;
    someRecordsDeleted          = true;
  }

  auto gotoFirst() -> bool {
    uint16_t idx = 0;
    while ((idx < recordCount) && isDeleted[idx]) idx++;
    if ((idx < recordCount) && !isDeleted[idx]) {
      currentRecordIdx = idx;
      return true;
    }
    return false;
  }

  auto gotoNext() -> bool {
    uint16_t idx = currentRecordIdx + 1;
    while ((idx < recordCount) && isDeleted[idx]) idx++;
    if ((idx < recordCount) && !isDeleted[idx]) {
      currentRecordIdx = idx;
      return true;
    }
    return false;
  }
};
