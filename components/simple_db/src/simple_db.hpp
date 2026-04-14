// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"
#include "himem.hpp"

#include <cstdio>

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

using SimpleDBPtr = himemUniquePtr<class SimpleDB>;

class SimpleDB {
private:
  static constexpr char const *TAG = "SimpleDB";

  static const uint16_t MAX_RECORD_COUNT = 1000;

  FILE *dbFile;

  bool dbIsOpen;
  bool someRecordsDeleted;
  int32_t recordOffset[MAX_RECORD_COUNT]; ///< record offset in file
  bool isDeleted[MAX_RECORD_COUNT];       ///< true if record is deleted
  uint16_t recordCount;
  uint32_t fileSize;
  uint16_t currentRecordIdx; ///< Index of current record in recordOffset

public:
  SimpleDB() : dbIsOpen(false), recordCount(0), currentRecordIdx(0) {};
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
  bool open(std::string filename);

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
  bool create(std::string filename);

  void close();

  inline uint16_t getCurrentIdx() { return currentRecordIdx; }
  inline void setCurrentIdx(int16_t index) { currentRecordIdx = index; }
  inline uint16_t getRecordCount() { return recordCount; }
  inline uint16_t getFileSize() { return fileSize; }
  inline bool someRecordsWereDeleted() { return someRecordsDeleted; }
  inline bool isDbOpen() { return dbIsOpen; }

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
  bool addRecord(void *record, int32_t size);

  bool getRecord(void *record, int32_t size);

  bool getPartialRecord(void *record, int32_t size, int32_t offset);

  void show();

  /**
   * @brief Get size of the current record.
   *
   * Returns 0 if at end of the database
   */
  int32_t getRecordSize() {
    if ((currentRecordIdx >= recordCount) || isDeleted[currentRecordIdx]) return 0;
    if (currentRecordIdx == (recordCount - 1)) {
      return fileSize - recordOffset[currentRecordIdx] - sizeof(int32_t);
    }
    return recordOffset[currentRecordIdx + 1] - recordOffset[currentRecordIdx] -
           sizeof(int32_t);
  }

  /**
   * @brief Set current record as deleted.
   *
   */
  void setDeleted() {
    isDeleted[currentRecordIdx] = true;
    someRecordsDeleted           = true;
  }

  bool gotoFirst() {
    uint16_t idx = 0;
    while ((idx < recordCount) && isDeleted[idx]) idx++;
    if ((idx < recordCount) && !isDeleted[idx]) {
      currentRecordIdx = idx;
      return true;
    }
    return false;
  }

  bool gotoNext() {
    uint16_t idx = currentRecordIdx + 1;
    while ((idx < recordCount) && isDeleted[idx]) idx++;
    if ((idx < recordCount) && !isDeleted[idx]) {
      currentRecordIdx = idx;
      return true;
    }
    return false;
  }
};
