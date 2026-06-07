// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __SIMPLE_DB__ 1
#include "simple_db.hpp"

#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// DMA-safe I/O helpers
// ---------------------------------------------------------------------------
#if EPUB_INKPLATE_BUILD
  #include "esp_heap_caps.h"
  #include "esp_memory_utils.h"
  // Cover bitmaps can be 50KB+; allocate a fixed bounce buffer and chunk the I/O
  // rather than trying to heap_caps_malloc the full record size at once.
  static constexpr size_t DMA_BOUNCE_SIZE = 4096;
  static auto fread_dma(void *dest, size_t elemSize, size_t count, FILE *f) -> size_t {
    if (esp_ptr_dma_capable(dest)) { return fread(dest, elemSize, count, f); }
    void *tmp = heap_caps_malloc(DMA_BOUNCE_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (tmp == nullptr) { return 0; }
    size_t   remaining = elemSize * count;
    uint8_t *d       = static_cast<uint8_t *>(dest);
    while (remaining > 0) {
      size_t chunk = remaining < DMA_BOUNCE_SIZE ? remaining : DMA_BOUNCE_SIZE;
      if (fread(tmp, 1, chunk, f) != chunk) {
        heap_caps_free(tmp);
        return 0;
      }
      std::memcpy(d, tmp, chunk);
      d += chunk;
      remaining -= chunk;
    }
    heap_caps_free(tmp);
    return count;
  }
  static auto fwrite_dma(const void *src, size_t elemSize, size_t count, FILE *f) -> size_t {
    if (esp_ptr_dma_capable(src)) { return fwrite(src, elemSize, count, f); }
    void *tmp = heap_caps_malloc(DMA_BOUNCE_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (tmp == nullptr) { return 0; }
    size_t         remaining = elemSize * count;
    const uint8_t *s = static_cast<const uint8_t *>(src);
    while (remaining > 0) {
      size_t chunk = remaining < DMA_BOUNCE_SIZE ? remaining : DMA_BOUNCE_SIZE;
      std::memcpy(tmp, s, chunk);
      if (fwrite(tmp, 1, chunk, f) != chunk) {
        heap_caps_free(tmp);
        return 0;
      }
      s += chunk;
      remaining -= chunk;
    }
    heap_caps_free(tmp);
    return count;
  }
#else
  static auto fread_dma(void *dest, size_t elemSize, size_t count, FILE *f) -> size_t {
    return fread(dest, elemSize, count, f);
  }
  static auto fwrite_dma(const void *src, size_t elemSize, size_t count, FILE *f) -> size_t {
    return fwrite(src, elemSize, count, f);
  }
#endif

auto SimpleDB::open(const HimemString &filename) -> bool {
  std::scoped_lock guard(mutex);
  LOG_D("Opening database file: {}", filename);

  if (dbIsOpen) {
    fclose(dbFile);
    dbIsOpen = false;
  }

  if ((dbFile = fopen(filename.c_str(), "r+")) == nullptr) { return create(filename); }

  bool        done = false;

  struct stat stat_buf;
  fstat(fileno(dbFile), &stat_buf);
  fileSize = stat_buf.st_size;

  uint16_t idx    = 0;
  uint32_t offset = 0;

  while ((idx < MAX_RECORD_COUNT) && (offset < fileSize)) {
    recordOffset[idx] = offset;
    isDeleted[idx]    = false;
    uint32_t size;
    if (fread(&size, sizeof(uint32_t), 1, dbFile) != 1) { goto error; }
    offset += size + sizeof(uint32_t);
    if (offset < fileSize) {
      if (fseek(dbFile, offset, SEEK_SET)) { goto error; } }
    idx++;
  }
  done        = true;
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
    LOG_D("Record count: {}", recordCount);
    return true;
  }
}

auto SimpleDB::create(const HimemString &filename) -> bool {
  std::scoped_lock guard(mutex);
  LOG_D("Creating database file: {}", filename);
  if (dbIsOpen) {
    fclose(dbFile);
    dbIsOpen = false;
  }

  if ((dbFile = fopen(filename.c_str(), "w+")) == nullptr) { return false; }

  dbIsOpen           = true;
  someRecordsDeleted = false;
  currentRecordIdx   = 0;
  recordCount        = 0;
  fileSize           = 0;

  return true;
}

auto SimpleDB::close() -> void {
  std::scoped_lock guard(mutex);
  if (dbIsOpen) {
    dbIsOpen = false;
    fclose(dbFile);
  }
}

auto SimpleDB::addRecord(void *record, int32_t size) -> bool {
  std::scoped_lock guard(mutex);
  LOG_D("Adding record of size {}", size);

  if (recordCount >= MAX_RECORD_COUNT) { return false; }
  if (fseek(dbFile, 0, SEEK_END)) { return false; }
  recordOffset[recordCount] = ftell(dbFile);
  isDeleted[recordCount++]  = false;
  if (fwrite(&size, sizeof(int32_t), 1, dbFile) != 1) { return false; }
  if ((size != 0) && (fwrite_dma(record, size, 1, dbFile) != 1)) { return false; }
  fflush(dbFile);
  fileSize += sizeof(int32_t) + size;
  return true;
}

auto SimpleDB::getRecord(void *record, int32_t size) -> bool {
  std::scoped_lock guard(mutex);
  // LOG_D("Reading record of size {}", size);

  if ((size <= 0) || (currentRecordIdx >= recordCount)) { return false; }
  if (fseek(dbFile, recordOffset[currentRecordIdx] + sizeof(int32_t), SEEK_SET)) { return false; }
  if (fread_dma(record, size, 1, dbFile) != 1) { return false; }
  return true;
}

auto SimpleDB::getPartialRecord(void *record, int32_t size, int32_t offset) -> bool {
  std::scoped_lock guard(mutex);
  // LOG_D("Reading partial record of size {} at offset {}", size, offset);

  if ((size <= 0) || (currentRecordIdx >= recordCount)) { return false; }
  if (fseek(dbFile, recordOffset[currentRecordIdx] + sizeof(int32_t) + offset, SEEK_SET)) {
    return false;
  }
  if (fread_dma(record, size, 1, dbFile) != 1) { return false; }
  return true;
}

auto SimpleDB::show() -> void {
  std::scoped_lock guard(mutex);
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