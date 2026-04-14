// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include <forward_list>
#include <mutex>

#define MINIZ 1

#include "himem.hpp"
#include "miniz.h"

using FileContentPtr = himemUniquePtr<uint8_t[]>;

class Unzip {
private:
  static constexpr char const *TAG = "Unzip";

  static const int BUFFER_SIZE = 1024 * 16;
  char buffer[BUFFER_SIZE];

  std::mutex mutex;

  /**
   * @brief File descriptor inside the zip file
   *
   */
  struct FileEntry {
    char *filename;
    uint32_t startPos;       // in zip file
    uint32_t compressedSize; // in zip file
    uint32_t size;            // once decompressed
    uint32_t currentPos;
    uint16_t method; // compress method (0 = not compressed, 8 = DEFLATE)
  };

  typedef std::forward_list<FileEntry *> FileEntries;
  FileEntries fileEntries;
  FileEntries::iterator currentFileEntry;

  uint32_t getUint32(const unsigned char *b) {
    return ((uint32_t)b[0]) | (((uint32_t)b[1]) << 8) | (((uint32_t)b[2]) << 16) |
           (((uint32_t)b[3]) << 24);
  }
  uint16_t getUint16(const unsigned char *b) { return ((uint32_t)b[0]) | (((uint32_t)b[1]) << 8); }

  uint16_t repeat;
  uint16_t remains;
  uint16_t current;
  bool aborted;

  FILE *file; // Current File Descriptor
  bool zipFileIsOpen;

  mz_stream zstr;

public:
  Unzip();
  bool seekToCentralDirectory();
  bool readFileEntries(uint32_t offset, uint16_t count);
  bool openZipFile(const char *zip_filename);
  void closeZipFile();

  int32_t getFileSize(const char *filename);
  auto getFile(const char *filename, uint32_t &fileSize) -> FileContentPtr;
  bool fileExists(const char *filename);
  bool openFile(const char *filename);
  void closeFile();

  #if !STB
    bool openStreamFile(const char *filename, uint32_t &fileSize);
    uint32_t getStreamData(char *data, uint32_t size);
    bool streamSkip(uint32_t byte_count);
    void closeStreamFile();
  #endif
};

#if __UNZIP__
  Unzip unzip;
#else
  extern Unzip unzip;
#endif
