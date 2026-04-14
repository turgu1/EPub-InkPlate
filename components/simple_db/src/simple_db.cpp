// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __SIMPLE_DB__ 1
#include "simple_db.hpp"

#include <inttypes.h>
#include <iostream>
#include <sys/stat.h>

bool SimpleDB::open(std::string filename) {
  LOG_D("Opening database file: %s", filename.c_str());

  if (dbIsOpen) {
    fclose(dbFile);
    dbIsOpen = false;
  }

  if ((dbFile = fopen(filename.c_str(), "r+")) == nullptr) return create(filename);

  bool done = false;

  struct stat stat_buf;
  fstat(fileno(dbFile), &stat_buf);
  fileSize = stat_buf.st_size;

  uint16_t idx    = 0;
  uint32_t offset = 0;

  while ((idx < MAX_RECORD_COUNT) && (offset < fileSize)) {
    recordOffset[idx] = offset;
    isDeleted[idx]    = false;
    uint32_t size;
    if (fread(&size, sizeof(uint32_t), 1, dbFile) != 1) goto error;
    offset += size + sizeof(uint32_t);
    if (offset < fileSize)
      if (fseek(dbFile, offset, SEEK_SET)) goto error;
    idx++;
  }
  done         = true;
  recordCount = idx;

error:
  if (!done) {
    fclose(dbFile);
    LOG_E("Database error!!");
    return false;
  } else {
    dbIsOpen           = true;
    someRecordsDeleted = false;
    currentRecordIdx   = 0;
    LOG_D("Record count: %d", recordCount);
    return true;
  }
}

bool SimpleDB::create(std::string filename) {
  LOG_D("Creating database file: %s", filename.c_str());
  if (dbIsOpen) {
    fclose(dbFile);
    dbIsOpen = false;
  }

  if ((dbFile = fopen(filename.c_str(), "w+")) == nullptr) return false;

  dbIsOpen           = true;
  someRecordsDeleted = false;
  currentRecordIdx   = 0;
  recordCount         = 0;
  fileSize            = 0;

  return true;
}

void SimpleDB::close() {
  if (dbIsOpen) {
    dbIsOpen = false;
    fclose(dbFile);
  }
}

bool SimpleDB::addRecord(void *record, int32_t size) {
  LOG_D("Adding record of size %" PRIi32, size);

  if (recordCount >= MAX_RECORD_COUNT) return false;
  if (fseek(dbFile, 0, SEEK_END)) return false;
  recordOffset[recordCount] = ftell(dbFile);
  isDeleted[recordCount++]  = false;
  if (fwrite(&size, sizeof(int32_t), 1, dbFile) != 1) return false;
  if ((size != 0) && (fwrite(record, size, 1, dbFile) != 1)) return false;
  fflush(dbFile);
  fileSize += sizeof(int32_t) + size;
  return true;
}

bool SimpleDB::getRecord(void *record, int32_t size) {
  // LOG_D("Reading record of size %d", size);

  if ((size <= 0) || (currentRecordIdx >= recordCount)) return false;
  if (fseek(dbFile, recordOffset[currentRecordIdx] + sizeof(int32_t), SEEK_SET)) return false;
  if (fread(record, size, 1, dbFile) != 1) return false;
  return true;
}

bool SimpleDB::getPartialRecord(void *record, int32_t size, int32_t offset) {
  // LOG_D("Reading partial record of size %d at offset %d", size, offset);

  if ((size <= 0) || (currentRecordIdx >= recordCount)) return false;
  if (fseek(dbFile, recordOffset[currentRecordIdx] + sizeof(int32_t) + offset, SEEK_SET))
    return false;
  if (fread(record, size, 1, dbFile) != 1) return false;
  return true;
}

void SimpleDB::show() {
  if (dbIsOpen) {
    std::cout << "===== Database content: ====" << std::endl;
    std::cout << "Record count: " << recordCount << std::endl;

    for (int16_t idx = 0; idx < recordCount; idx++) {
      std::cout << idx << ":"
                << " offset: " << recordOffset[idx]
                << " size: " << recordOffset[idx + 1] - recordOffset[idx] - sizeof(int32_t)
                << (isDeleted[idx] ? " DELETED" : "") << std::endl;
    }

    std::cout << "===== End of database =====" << std::endl;
  }
}