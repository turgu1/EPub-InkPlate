// Copyright (c) 2020 Guy Turcotte
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include <forward_list>
#include <fstream>
#include <mutex>
#include <memory>

#define MINIZ 1

#include "char_pool.hpp"
#include "himem.hpp"
#include "himem_pool.hpp"
#include "himem_simple_list.hpp"
#include "miniz.h"

class Unzip {
  public:
    // Forward declaration of the safe stream context
    struct StreamSession {
      uint16_t repeat{ 0 };
      uint16_t remains{ 0 };
      uint16_t current{ 0 };
      bool aborted{ false };
      bool streamInflateInitialized{ false };
      mz_stream zstr{};
    };

  private:
    static constexpr char const *TAG = "Unzip";

    struct FilenamePoolTag;
    using ZipFilename =
      std::basic_string<char, std::char_traits<char>, StaticPoolAllocator<char, FilenamePoolTag> >;

    static const int BUFFER_SIZE = 1024 * 16;
    char buffer[BUFFER_SIZE]; // DMA Target for SPI/SD reads

    // Using recursive_mutex handles reentrancy automatically and safely
    std::recursive_mutex mutex;

    class FileEntry {
      public:
        ZipFilename filename{};
        uint32_t startPos{ 0 };
        uint32_t compressedSize{ 0 };
        uint32_t size{ 0 };
        uint32_t currentPos{ 0 };
        uint16_t method{ 0 };
        FileEntry()  = default;
        ~FileEntry() = default;
    };

    using FileEntries = HimemSimpleList<FileEntry>;
    FileEntries fileEntries{};
    FileEntries::iterator currentFileEntry{};
    CharPoolPtr filenamePool{};
    uint32_t filenameEntryCount{ 0 };
    uint32_t totalFilenameBytes{ 0 };
    uint16_t maxFilenameSize{ 0 };

    auto getUint32(const unsigned char *b) -> uint32_t {
      return ((uint32_t)b[0]) | (((uint32_t)b[1]) << 8) | (((uint32_t)b[2]) << 16) | (((uint32_t)b[3]) << 24);
    }
    auto getUint16(const unsigned char *b) -> uint16_t {
      return ((uint32_t)b[0]) | (((uint32_t)b[1]) << 8);
    }

    std::ifstream file{};
    bool zipFileIsOpen{ false };
    std::string currentFilename{};

    // Added a session pointer to manage active sessions safely across methods
    std::unique_ptr<StreamSession> activeSession{ nullptr };

    std::unique_lock<std::recursive_mutex> streamLock{};

    auto closeZipFileUnsafe() -> void;
    static bool alive;

  public:
    Unzip() { alive = true; };
    ~Unzip() {
      closeZipFile();
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
