// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __UNZIP__ 1
#include "unzip.hpp"

#include "himem.hpp"

#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <fcntl.h>
#include <iomanip>
#include <memory>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if DEBUGGING
  #include <iostream>
#endif

bool         Unzip::alive = false;

static void *zipMem = nullptr;

static auto myZAlloc(void *opaque, size_t items, size_t size) -> void * {
  if (zipMem != nullptr) {
    return zipMem;
  } else {
    #if EPUB_INKPLATE_BUILD
      return zipMem = heap_caps_malloc(items * size, MALLOC_CAP_SPIRAM);
    #else
      // const char *TAG = "myZAlloc";
      // LOG_I("Allocating {} bytes for zlib", items * size);
      return zipMem = malloc(items * size);
    #endif
  }
}

static void myZFree(void *opaque, void *address) {
// Do nothing
  #if 0
    #if EPUB_INKPLATE_BUILD
      heap_caps_free(address);
    #else
      const char *TAG = "myZFree";
      LOG_I("Freeing memory for zlib");
      free(address);
    #endif
  #endif
}

/**
 * @brief Seeks to the central directory of a ZIP file.
 *
 * Locates the "End Of Central Directory" (EOCD) record in the ZIP file by searching
 * backwards from the end of the file for the EOCD signature "PK\5\6" (0x06054b50).
 *
 * The function first attempts to find the EOCD record at the expected location
 * (FILE_CENTRAL_SIZE bytes before the end of file). If the signature is not found there,
 * it performs a backward search within the last 65536 bytes of the file to account for
 * ZIP file comments.
 *
 * Upon successful location of the EOCD record, the file pointer is positioned at the
 * beginning of the EOCD record and the record data is read into the internal buffer.
 *
 * The EOCD record structure contains:
 * - End of central directory signature (4 bytes): 0x06054b50
 * - Disk number (2 bytes)
 * - Disk with central directory start (2 bytes)
 * - Number of entries on this disk (2 bytes)
 * - Total number of entries (2 bytes)
 * - Central directory size (4 bytes)
 * - Central directory offset (4 bytes)
 * - Comment length (2 bytes)
 * - Comment data (variable size)
 *
 * @return bool True if the central directory was successfully located and its
 *              EOCD record was read into the buffer; false otherwise.
 *
 * @note A failure to locate the EOCD record will log an error message.
 */
auto Unzip::seekToCentralDirectory() -> bool {

  if (!zipFileIsOpen || !file.is_open()) {
    LOG_E("seekToCentralDirectory called with invalid zip state.");
    return false;
  }

  const int FILE_CENTRAL_SIZE = 22;

  buffer[FILE_CENTRAL_SIZE] = 0;
  bool found                = false;

  auto seekAbs = [this](std::streamoff pos) -> bool {
                   file.clear();
                   file.seekg(pos, std::ios::beg);
                   return file.good();
                 };

  auto readExact = [this](void *dst, std::size_t size) -> bool {
                     if (size == 0) { return true; }
                     file.read(static_cast<char *>(dst), static_cast<std::streamsize>(size));
                     return file.good();
                   };

  file.clear();
  file.seekg(0, std::ios::end);
  std::streamoff length = static_cast<std::streamoff>(file.tellg());
  if (length >= 0) {
    if (length >= FILE_CENTRAL_SIZE) {
      std::streamoff offset = length - FILE_CENTRAL_SIZE;

      if (seekAbs(offset) && readExact(buffer, FILE_CENTRAL_SIZE)) {
        if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 5) && (buffer[3] == 6))) {
          // There must be a comment in the last entry. Search for the beginning of the entry
          std::streamoff endOffset = offset - 65536;
          if (endOffset < 0) { endOffset = 0; }
          offset -= FILE_CENTRAL_SIZE;
          while (!found && (offset > endOffset)) {
            if (seekAbs(offset) && readExact(buffer, FILE_CENTRAL_SIZE + 5)) {
              char *p;
              if ((p = (char *)memmem(buffer, FILE_CENTRAL_SIZE + 5, "PK\5\6", 4)) != nullptr) {
                offset += (p - buffer);
                found = seekAbs(offset) && readExact(buffer, FILE_CENTRAL_SIZE);
                break;
              }
              offset -= FILE_CENTRAL_SIZE;
            } else {
              break;
            }
          }
        } else {
          found = true;
        }
      }
    }
  }
  if (!found) {
    LOG_E("Unable to find central directory in zip file.");
  }
  return found;
}

/**
 * @brief Reads file entries from the ZIP central directory.
 *
 * This method parses the central directory of a ZIP file starting at the specified
 * offset and reads the requested number of file entry headers. Each file entry contains
 * metadata such as filename, compression method, file sizes, and the local header offset.
 *
 * @param offset The file offset where the central directory begins
 * @param count The number of file entries to read (currently unused in implementation)
 *
 * @return true if the central directory was completely and successfully read,
 *         false if an error occurred during reading or parsing
 *
 * @details
 * The method reads ZIP central file header structures (42 bytes fixed + variable data):
 * - Validates each entry with the central directory signature (0x02014b50 / "PK\x01\x02")
 * - Extracts filename, compression method, CRC-32, compressed/uncompressed sizes
 * - Extracts the relative offset of the local file header
 * - Dynamically allocates FileEntry objects and stores them in fileEntries list
 * - Skips extra fields and file comments as defined in each entry's metadata
 *
 * @note
 * - Allocates memory dynamically for each filename and FileEntry object
 * - Stores entries in a front-insertion container (push_front)
 * - Logs errors if the central directory cannot be completely read
 *
 * @see FileEntry, getUint16(), getUint32()
 */
auto Unzip::readFileEntries(uint32_t offset, uint16_t count) -> bool {

  if (!zipFileIsOpen || !file.is_open()) {
    LOG_E("readFileEntries called with invalid zip state.");
    return false;
  }

  // Central Directory record structure:

  // [file header 1]
  // .
  // .
  // .
  // [file header n]
  // [digital signature] // PKZip 6.2 or later only

  // File header:

  // central file header signature   4 bytes  (0x02014b50)
  // version made by                 2 bytes   0
  // version needed to extract       2 bytes   2
  // general purpose bit flag        2 bytes   4
  // compression method              2 bytes   6
  // last mod file time              2 bytes   8
  // last mod file date              2 bytes  10
  // crc-32                          4 bytes  12
  // compressed size                 4 bytes  16
  // uncompressed size               4 bytes  20
  // file name length                2 bytes  24
  // extra field length              2 bytes  26
  // file comment length             2 bytes  28
  // disk number start               2 bytes  30
  // internal file attributes        2 bytes  32
  // external file attributes        4 bytes  34
  // relative offset of local header 4 bytes  38

  // file name (variable size)
  // extra field (variable size)
  // file comment (variable size)

  const int FILE_ENTRY_SIZE = 42;

  bool      completed = false;

  auto      seekAbs = [this](std::streamoff pos) -> bool {
                        file.clear();
                        file.seekg(pos, std::ios::beg);
                        return file.good();
                      };

  auto readExact = [this](void *dst, std::size_t size) -> bool {
                     if (size == 0) { return true; }
                     file.read(static_cast<char *>(dst), static_cast<std::streamsize>(size));
                     return file.good();
                   };

  if ((count > 0) && seekAbs(offset)) {

    while (true) {
      if (!readExact(buffer, 4)) { break; }
      if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 1) && (buffer[3] == 2))) {
        // End of list...
        completed = true;
        break;
      }
      if (!readExact(buffer, FILE_ENTRY_SIZE)) { break; }

      uint16_t filename_size = getUint16((const unsigned char *)&buffer[24]);
      uint16_t extra_size    = getUint16((const unsigned char *)&buffer[26]);
      uint16_t comment_size  = getUint16((const unsigned char *)&buffer[28]);

      if (filename_size > CharPool::MAX_ALLOC - 1) {
        LOG_E("Filename too long for CharPool: {}", filename_size);
        break;
      }

      fileEntries.emplace_back();
      auto &fe = fileEntries.back();
      fe.filename.resize(filename_size);
      if (!readExact(fe.filename.data(), filename_size)) {
        LOG_E("Unable to read filename from central directory entry.");
        fileEntries.removeLast();
        break;
      }

      fe.startPos       = getUint32((const unsigned char *)&buffer[38]);
      fe.compressedSize = getUint32((const unsigned char *)&buffer[16]);
      fe.size           = getUint32((const unsigned char *)&buffer[20]);
      fe.method         = getUint16((const unsigned char *)&buffer[6]);
      ++filenameEntryCount;
      totalFilenameBytes += filename_size;
      if (filename_size > maxFilenameSize) { maxFilenameSize = filename_size; }

      // LOG_D("File: {} {} {} {} {}", fe.filename, fe.startPos, fe.compressedSize,
      // fe.size, fe.method);
      offset += FILE_ENTRY_SIZE + 4 + filename_size + extra_size + comment_size;
      if (!seekAbs(offset)) { break; }
    }
  }
  if (!completed) {
    LOG_E("Unable to read central directory.");
  }

  return completed;
}

auto Unzip::openZipFile(const char *zipFilename) -> bool {
  std::scoped_lock guard(mutex);

  // Open zip file
  if (zipFileIsOpen) {
    if (currentFilename == zipFilename) { return true; }
    closeZipFileUnsafe();
  }

  file.open(zipFilename, std::ios::binary);
  if (!file.is_open()) {
    LOG_E("Unable to open file: {}", zipFilename);
    return false;
  }

  filenamePool = CharPool::Make();
  if (filenamePool == nullptr) {
    LOG_E("Unable to allocate filename CharPool.");
    file.close();
    return false;
  }
  PoolContext<FilenamePoolTag>::init(filenamePool.get());
  filenameEntryCount = 0;
  totalFilenameBytes = 0;
  maxFilenameSize    = 0;

  zipFileIsOpen   = true;
  currentFilename = zipFilename;

  bool completed =
    seekToCentralDirectory() && readFileEntries(getUint32((const unsigned char *)&buffer[16]),
                                                getUint16((const unsigned char *)&buffer[10]));

  if (!completed) {
    closeZipFileUnsafe();
  } else {
    // LOG_I("ZIP entries={} filenameBytes={} maxFilename={}", filenameEntryCount,
    // totalFilenameBytes,
    //       maxFilenameSize);
    if (filenamePool != nullptr) {
      [[maybe_unused]] size_t allocated = filenamePool->getTotalAllocated();
      [[maybe_unused]] size_t freed     = filenamePool->getTotalFreed();
      // LOG_I("Filename CharPool: alloc={} freed={} live={}", allocated, freed, allocated -
      // freed);
    }
    LOG_D("openZipFile completed!");
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto &f : fileEntries) {
        std::cout << "pos: " << std::setw(7) << f.startPos << " zip size: " << std::setw(7)
                  << f.compressedSize << " out size: " << std::setw(7) << f.size
                  << " method: " << std::setw(1) << f.method << " name: " << f.filename << std::endl;
      }
      std::cout << "[End of List]" << std::endl;
    #endif
  }

  return completed;
}

auto Unzip::closeZipFileUnsafe() -> void {
  if (streamInflateInitialized) {
    mz_inflateEnd(&zstr);
    streamInflateInitialized = false;
  }

  if (zipFileIsOpen) {
    if (filenamePool != nullptr) {
      [[maybe_unused]] size_t allocated = filenamePool->getTotalAllocated();
      [[maybe_unused]] size_t freed     = filenamePool->getTotalFreed();
      // LOG_I("Closing ZIP: entries={} filenameBytes={} maxFilename={} pool alloc={} freed={} "
      //       "live={}",
      //       filenameEntryCount, totalFilenameBytes, maxFilenameSize, allocated, freed,
      //       allocated - freed);
    }

    fileEntries.clear();
    PoolContext<FilenamePoolTag>::reset();
    filenamePool.reset();
    filenameEntryCount = 0;
    totalFilenameBytes = 0;
    maxFilenameSize    = 0;
    if (file.is_open()) { file.close(); }
    zipFileIsOpen = false;
  }
  currentFileEntry = fileEntries.end();
  currentFilename.clear();
  streamMutexHeld = false;
  aborted         = false;

  #if EPUB_LINUX_BUILD
    streamOwnerThread = std::thread::id{};
  #else
    streamOwnerThread = nullptr;
  #endif
}

auto Unzip::closeZipFile() -> void {
  #if EPUB_LINUX_BUILD
    if (std::this_thread::get_id() == streamOwnerThread) {
      // If the current thread is the stream owner, avoid locking the mutex again to prevent deadlock.
      closeZipFileUnsafe();
      return;
    }
  #else
    if (xTaskGetCurrentTaskHandle() == streamOwnerThread) {
      // If the current thread is the stream owner, avoid locking the mutex again to prevent deadlock.
      closeZipFileUnsafe();
      return;
    }
  #endif

  std::scoped_lock guard(mutex);
  closeZipFileUnsafe();

  // LOG_D("Zip file closed.");
}

/**
 * @brief Clean filename path
 *
 * This function cleans filename path that may contain relation folders (like '..').
 *
 * @param filename The filename to clean
 * @return char * The cleaned up filename. Must be freed after usage.
 */
auto cleanFname(const char *filename) -> std::unique_ptr<char[]> {
  auto        str = std::make_unique<char[]>(strlen(filename) + 1);

  const char *s = filename;
  const char *u;
  char *      t = str.get();

  while ((u = strstr(s, "/../")) != nullptr) {
    const char *ss = s;     // keep it for copy in target
    s              = u + 3; // prepare for next iteration
    do {                    // get rid of preceeding folder name
      --u;
    } while ((u > ss) && (*u != '/'));
    if (u >= ss) {
      while (ss != u) *t++ = *ss++;
    } else if ((*u != '/') && (t > str.get())) {
      do {
        --t;
      } while ((t > str.get()) && (*t != '/'));
    }
  }
  if ((t == str.get()) && (*s == '/')) { ++s; }
  while ((*t++ = *s++));

  return str;
}

auto Unzip::getFileSize(const char *filename) -> int32_t {

  std::scoped_lock guard(mutex);

  if (!zipFileIsOpen) { return 0; }

  auto theFilename = cleanFname(filename);
  currentFileEntry = fileEntries.begin();

  while (currentFileEntry != fileEntries.end()) {
    if (currentFileEntry->filename == theFilename.get()) { break; }
    ++currentFileEntry;
  }

  int32_t size = 0;

  if (currentFileEntry == fileEntries.end()) {
    LOG_E("Unzip getFileSize: File not found: {}", theFilename.get());
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto &f : fileEntries) std::cout << "  <" << f.filename << ">" << std::endl;
      std::cout << "[End of List]" << std::endl;
    #endif
  } else {
    size = currentFileEntry->size;
  }

  return size;
}

auto Unzip::fileExists(const char *filename) -> bool {
  std::scoped_lock guard(mutex);

  if (!zipFileIsOpen) { return false; }

  auto                         theFilename = cleanFname(filename);

  Unzip::FileEntries::iterator fe = fileEntries.begin();

  while (fe != fileEntries.end()) {
    if (fe->filename == theFilename.get()) { break; }
    ++fe;
  }

  return fe != fileEntries.end();
}

auto Unzip::openFile(const char *filename) -> bool {

  int err = 0;

  if (!zipFileIsOpen) { return false; }
  if (!file.is_open()) {
    LOG_E("Unzip openFile called with null file handle.");
    return false;
  }

  auto theFilename = cleanFname(filename);
  currentFileEntry = fileEntries.begin();

  while (currentFileEntry != fileEntries.end()) {
    if (currentFileEntry->filename == theFilename.get()) { break; }
    ++currentFileEntry;
  }

  if (currentFileEntry == fileEntries.end()) {
    LOG_E("Unzip Get: File not found: {}", theFilename.get());
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto &f : fileEntries) std::cout << "  <" << f.filename << ">" << std::endl;
      std::cout << "[End of List]" << std::endl;
    #endif
    return false;
  }
  // else {
  //   LOG_D("File: {} at pos: {}", (*fe)->filename, (*fe)->startPos);
  // }

  bool completed = false;

  // Local header record.

  // local file header signature     4 bytes  (0x04034b50)
  // version needed to extract       2 bytes   0
  // general purpose bit flag        2 bytes   2
  // compression method              2 bytes   4
  // last mod file time              2 bytes   6
  // last mod file date              2 bytes   8
  // crc-32                          4 bytes  10
  // compressed size                 4 bytes  14
  // uncompressed size               4 bytes  18
  // file name length                2 bytes  22
  // extra field length              2 bytes  24

  // file name (variable size)
  // extra field (variable size)

  const int LOCAL_HEADER_SIZE = 26;

  auto      seekAbs = [this](std::streamoff pos) -> bool {
                        file.clear();
                        file.seekg(pos, std::ios::beg);
                        if (!file.good()) {
                          LOG_E("Error seeking to file data at pos: {}", pos);
                        }
                        return file.good();
                      };

  auto readExact = [this](void *dst, std::size_t size) -> bool {
                     if (size == 0) { return true; }
                     file.read(static_cast<char *>(dst), static_cast<std::streamsize>(size));
                     if (!file.good()) {
                       LOG_E("Error reading file data at pos: {}", (int)file.tellg());
                     }
                     return file.good();
                   };

  if (seekAbs(currentFileEntry->startPos) && readExact(buffer, 4)) {

    if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 3) && (buffer[3] == 4))) {
      err = 15;
    } else if (!readExact(buffer, LOCAL_HEADER_SIZE)) {
      err = 16;
    } else {
      uint16_t       filename_size = getUint16((const unsigned char *)&buffer[22]);
      uint16_t       extra_size    = getUint16((const unsigned char *)&buffer[24]);

      std::streamoff headerEndPos = static_cast<std::streamoff>(file.tellg());
      if (headerEndPos < 0) {
        err = 18;
      } else {
        std::streamoff dataPos = headerEndPos + static_cast<std::streamoff>(filename_size) +
                                 static_cast<std::streamoff>(extra_size);
        if (!seekAbs(dataPos)) {
          err = 17;
        } else {
          completed = true;
        }
      }
    }
  } else {
    err = 13;
  }
  if (completed) {
    currentFileEntry->currentPos = 0;
    return true;
  } else {
    LOG_E("Unzip openFile: Error!: {}", err);
    return false;
  }
}

auto Unzip::closeFile() -> void {}

#if !STB

  auto Unzip::openStreamFile(const char *filename, uint32_t &fileSize) -> bool {

    #if EPUB_LINUX_BUILD
      if (streamMutexHeld && (streamOwnerThread == std::this_thread::get_id())) {
        LOG_E("openStreamFile called while a stream is already open on this thread.");
        return false;
      }
    #else
      if (streamMutexHeld && (streamOwnerThread == xTaskGetCurrentTaskHandle())) {
        LOG_E("openStreamFile called while a stream is already open on this thread.");
        return false;
      }
    #endif

    mutex.lock();
    streamMutexHeld = true;

    #if EPUB_LINUX_BUILD
      streamOwnerThread = std::this_thread::get_id();
    #else
      streamOwnerThread = xTaskGetCurrentTaskHandle();
    #endif

    if (!openFile(filename)) {
      streamMutexHeld = false;
      #if EPUB_LINUX_BUILD
        streamOwnerThread = std::thread::id{};
      #else
        streamOwnerThread = nullptr;
      #endif
      mutex.unlock();
      return false;
    }

    repeat  = currentFileEntry->compressedSize / BUFFER_SIZE;
    remains = currentFileEntry->compressedSize % BUFFER_SIZE;
    current = 0;
    aborted = false;

    zstr.zalloc    = myZAlloc;
    zstr.zfree     = myZFree;
    zstr.opaque    = nullptr;
    zstr.next_in   = nullptr;
    zstr.next_out  = nullptr;
    zstr.avail_in  = 0;
    zstr.avail_out = 0;

    int zret;

    if ((zret = mz_inflateInit2(&zstr, -15)) != MZ_OK) {
      LOG_E("Error initializing zlib inflate: {}", zret);
      aborted = true;
      closeStreamFile();
      return false;
    }
    streamInflateInitialized = true;

    fileSize = currentFileEntry->size;

    auto readExact = [this](void *dst, std::size_t size) -> bool {
                       if (size == 0) { return true; }
                       file.read(static_cast<char *>(dst), static_cast<std::streamsize>(size));
                       if (!file.good()) {
                         LOG_E("Error reading zip content at pos: {}", (int)file.tellg());
                       }
                       return file.good();
                     };

    uint16_t size = current < repeat ? BUFFER_SIZE : remains;
    if (!readExact(buffer, size)) {
      LOG_E("Error reading zip content.");
      closeStreamFile();
      aborted = true;
      return false;
    }

    ++current;

    zstr.avail_in = size;
    zstr.next_in  = (unsigned char *)buffer;

    return true;
  }

  auto Unzip::closeStreamFile() -> void {
    if (!streamMutexHeld) { return; }

    #if EPUB_LINUX_BUILD
      if (streamOwnerThread != std::this_thread::get_id()) {
        LOG_E("closeStreamFile called from a non-owner thread.");
        return;
      }
    #else
      if (streamOwnerThread != xTaskGetCurrentTaskHandle()) {
        LOG_E("closeStreamFile called from a non-owner thread.");
        return;
      }
    #endif

    if (streamInflateInitialized) {
      mz_inflateEnd(&zstr);
      streamInflateInitialized = false;
    }

    closeFile();

    streamMutexHeld = false;
    #if EPUB_LINUX_BUILD
      streamOwnerThread = std::thread::id{};
    #else
      streamOwnerThread = nullptr;
    #endif
    aborted = false;

    mutex.unlock();
  }

  auto Unzip::streamSkip(uint32_t byteCount) -> bool {
    auto     tmp      = makeUniqueHimem<char[]>(byteCount);
    uint32_t size = byteCount;

    do {
      uint32_t s = size;
      if ((s = getStreamData(tmp.get(), s)) == 0) {
        return false;
      }
      size -= s;
    } while (size > 0);
    return true;
  }

  auto Unzip::getStreamData(char *data, uint32_t dataSize) -> uint32_t {

    if (dataSize == 0) { return 0; }

    #if EPUB_LINUX_BUILD
      if (!streamMutexHeld || (streamOwnerThread != std::this_thread::get_id()) || !file.is_open()) {
        LOG_E("Unzip getStreamData called outside stream session.");
        aborted = true;
        return 0;
      }
    #else
      if (!streamMutexHeld || (streamOwnerThread != xTaskGetCurrentTaskHandle()) || !file.is_open()) {
        LOG_E("Unzip getStreamData called outside stream session.");
        aborted = true;
        return 0;
      }
    #endif

    auto readExact = [this](void *dst, std::size_t size) -> bool {
                       if (size == 0) { return true; }
                       file.read(static_cast<char *>(dst), static_cast<std::streamsize>(size));
                       return file.good();
                     };

    zstr.next_out  = (unsigned char *)data;
    zstr.avail_out = dataSize;

    if (currentFileEntry->method == 0) {
      while (!aborted && (zstr.avail_out > 0)) {
        uint16_t copy_size = zstr.avail_in <= zstr.avail_out ? zstr.avail_in : zstr.avail_out;
        memcpy(zstr.next_out, zstr.next_in, copy_size);

        zstr.next_out += copy_size;
        zstr.next_in += copy_size;
        zstr.avail_out -= copy_size;
        zstr.avail_in -= copy_size;

        if (zstr.avail_in == 0) {
          if (current > repeat) { break; } // We are at the end
          uint16_t size = current < repeat ? BUFFER_SIZE : remains;
          if (!readExact(buffer, size)) {
            LOG_E("Error reading zip content.");
            aborted = true;
            break;
          }

          ++current;

          zstr.avail_in = size;
          zstr.next_in  = (unsigned char *)buffer;
        }
      }
    } else if (currentFileEntry->method == 8) {

      while (!aborted && (zstr.avail_out == dataSize)) {

        int zret = mz_inflate(&zstr, MZ_NO_FLUSH);

        switch (zret) {
        case MZ_NEED_DICT:
        case MZ_DATA_ERROR:
        case MZ_MEM_ERROR:
          LOG_E("Error inflating data: {}", zret);
          aborted = true;
        default:;
        }

        if (!aborted && (zstr.avail_out != 0)) {
          if ((zstr.avail_in == 0) && (current <= repeat)) {
            uint16_t size = current < repeat ? BUFFER_SIZE : remains;
            if (!readExact(buffer, size)) {
              LOG_E("Error reading zip content.");
              aborted = true;
            } else {
              ++current;

              zstr.avail_in = size;
              zstr.next_in  = (unsigned char *)buffer;
            }
          }
        }
      }
    }

    return !aborted ? dataSize - zstr.avail_out : 0;
  }

// Test version of getFile using stream methods
  auto Unzip::getFile(const char *filename, uint32_t &fileSize) -> FileContentPtr {
    // LOG_D("getFile: {}", filename);

    uint32_t       total = 0;

    bool           completed     = false;
    bool           stream_opened = false;

    FileContentPtr data{ nullptr };

    if (!openStreamFile(filename, fileSize)) {
      LOG_E("Unable to retrieve file {}", filename);
    } else {
      stream_opened = true;

      if ((data = makeUniqueHimem<uint8_t[]>(fileSize + 1)) == nullptr) {
        #if EPUB_INKPLATE_BUILD
          uint32_t spiramFree    = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
          uint32_t spiramLargest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
          LOG_E("Not enough memory to retrieve file {} (need {} B;"
                " SPIRAM free={} B largest={} B)",
                filename, fileSize, spiramFree, spiramLargest);
        #else
          LOG_E("Not enough memory to retrieve file {} ({} bytes)", filename, fileSize);
        #endif
      } else {
        data[fileSize] = 0;

        uint8_t *data_ptr = data.get();
        uint32_t size     = fileSize;

        while (((size = getStreamData((char *)data_ptr, size)) != 0) &&
               ((total + size) <= fileSize)) {

          data_ptr += size;
          total += size;

          LOG_D("Got {} bytes...", size);

          if (total == fileSize) { break; }

          size = fileSize - total;
        }

        LOG_D("File size: {}, received: {}", fileSize, total);
        completed = true;
      }
    }

    if (stream_opened) { closeStreamFile(); }

    if (!completed) {
      fileSize = 0;
      return nullptr;
    } else {
      fileSize = total;
    }

    return data;
  }

#endif