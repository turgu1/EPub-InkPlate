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

class Unzip {
private:
  static constexpr char const *TAG = "Unzip";

  static const int BUFFER_SIZE = 1024 * 16;
  HimemUniquePtr<char[]> bufStorage{};

  std::mutex mutex;

  /**
   * @brief File descriptor inside the zip file
   *
   */
  struct FileEntry {
    char *filename;
    uint32_t startPos;       // in zip file
    uint32_t compressedSize; // in zip file
    uint32_t size;           // once decompressed
    uint32_t currentPos;
    uint16_t method; // compress method (0 = not compressed, 8 = DEFLATE)
  };

  typedef std::forward_list<FileEntry *> FileEntries;
  FileEntries fileEntries;
  FileEntries::iterator currentFileEntry{};

  auto getUint32(const unsigned char *b) -> uint32_t {
    return ((uint32_t)b[0]) | (((uint32_t)b[1]) << 8) | (((uint32_t)b[2]) << 16) |
           (((uint32_t)b[3]) << 24);
  }
  auto getUint16(const unsigned char *b) -> uint16_t {
    return ((uint32_t)b[0]) | (((uint32_t)b[1]) << 8);
  }

  uint16_t repeat{0};
  uint16_t remains{0};
  uint16_t current{0};
  bool aborted{false};

  FILE *file{nullptr}; // Current File Descriptor
  bool zipFileIsOpen;
  std::string currentFilename;

  mz_stream zstr{};

public:
  Unzip();
  auto seekToCentralDirectory() -> bool;
  auto readFileEntries(uint32_t offset, uint16_t count) -> bool;
  auto openZipFile(const char *zipFilename) -> bool;
  auto closeZipFile() -> void;

  auto getFileSize(const char *filename) -> int32_t;
  auto getFile(const char *filename, uint32_t &fileSize) -> FileContentPtr;
  auto fileExists(const char *filename) -> bool;
  auto openFile(const char *filename) -> bool;
  auto closeFile() -> void;

  #if !STB
    auto openStreamFile(const char *filename, uint32_t &fileSize) -> bool;
    auto getStreamData(char *data, uint32_t size) -> uint32_t;
    auto streamSkip(uint32_t byteCount) -> bool;
    auto closeStreamFile() -> void;
  #endif
};

#if __UNZIP__
  Unzip unzip;
#else
  extern Unzip unzip;
#endif
