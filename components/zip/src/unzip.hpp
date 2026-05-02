// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include <forward_list>
#include <fstream>
#include <mutex>

#define MINIZ 1

#include "himem.hpp"
#include "himem_pool.hpp"
#include "miniz.h"

class Unzip {
private:
  static constexpr char const *TAG = "Unzip";

  static const int BUFFER_SIZE = 1024 * 16;
  char buffer[BUFFER_SIZE]; // Must stay in internal DRAM (DMA target for SPI/SD reads)

  std::mutex mutex;

  /**
   * @brief File descriptor inside the zip file
   *
   */
  class FileEntry {
  public:
    HimemString filename{};
    uint32_t startPos{0};       // in zip file
    uint32_t compressedSize{0}; // in zip file
    uint32_t size{0};           // once decompressed
    uint32_t currentPos{0};
    uint16_t method{0}; // compress method (0 = not compressed, 8 = DEFLATE)
    FileEntry()  = default;
    ~FileEntry() = default;
  };

  using FileEntries = std::forward_list<FileEntry, HimemPool<FileEntry>>;
  FileEntries fileEntries{};
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

  std::ifstream file{}; // Current zip file stream

  bool zipFileIsOpen{false};
  std::string currentFilename{};
  bool streamMutexHeld{false};
  bool streamInflateInitialized{false};

  mz_stream zstr{};

  auto closeZipFileUnsafe() -> void;

  static bool alive;

public:
  Unzip() { alive = true; };
  ~Unzip() {
    closeZipFileUnsafe();
    alive = false;
  };

  static inline auto isAlive() -> bool { return alive; }

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
