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
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if DEBUGGING
  #include <iostream>
#endif

Unzip::Unzip() { zip_file_is_open = false; }

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
bool Unzip::seek_to_central_directory() {

  const int FILE_CENTRAL_SIZE = 22;

  buffer[FILE_CENTRAL_SIZE] = 0;
  bool found                = false;

  if (fseek(file, 0, SEEK_END) == 0) {
    off_t length = ftell(file);
    if (length >= FILE_CENTRAL_SIZE) {
      off_t offset = length - FILE_CENTRAL_SIZE;

      if ((fseek(file, offset, SEEK_SET) == 0) &&
          (fread(buffer, FILE_CENTRAL_SIZE, 1, file) == 1)) {
        if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 5) && (buffer[3] == 6))) {
          // There must be a comment in the last entry. Search for the beginning of the entry
          off_t end_offset = offset - 65536;
          if (end_offset < 0) end_offset = 0;
          offset -= FILE_CENTRAL_SIZE;
          while (!found && (offset > end_offset)) {
            if ((fseek(file, offset, SEEK_SET) == 0) &&
                (fread(buffer, FILE_CENTRAL_SIZE + 5, 1, file) == 1)) {
              char *p;
              if ((p = (char *)memmem(buffer, FILE_CENTRAL_SIZE + 5, "PK\5\6", 4)) != nullptr) {
                offset += (p - buffer);
                found = (fseek(file, offset, SEEK_SET) == 0) &&
                        (fread(buffer, FILE_CENTRAL_SIZE, 1, file) == 1);
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
 * - Dynamically allocates FileEntry objects and stores them in file_entries list
 * - Skips extra fields and file comments as defined in each entry's metadata
 *
 * @note
 * - Allocates memory dynamically for each filename and FileEntry object
 * - Stores entries in a front-insertion container (push_front)
 * - Logs errors if the central directory cannot be completely read
 *
 * @see FileEntry, getuint16(), getuint32()
 */
bool Unzip::read_file_entries(uint32_t offset, uint16_t count) {

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

  bool completed = false;

  if ((count > 0) && (fseek(file, offset, SEEK_SET) == 0)) {

    while (true) {
      if (fread(buffer, 4, 1, file) != 1) break;
      if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 1) && (buffer[3] == 2))) {
        // End of list...
        completed = true;
        break;
      }
      if (fread(buffer, FILE_ENTRY_SIZE, 1, file) != 1) break;

      uint16_t filename_size = getuint16((const unsigned char *)&buffer[24]);
      uint16_t extra_size    = getuint16((const unsigned char *)&buffer[26]);
      uint16_t comment_size  = getuint16((const unsigned char *)&buffer[28]);

      char *fname = new char[filename_size + 1];
      if (fread(fname, filename_size, 1, file) != 1) {
        delete[] fname;
        break;
      }
      fname[filename_size] = 0;

      FileEntry *fe = new FileEntry;

      fe->filename        = fname;
      fe->start_pos       = getuint32((const unsigned char *)&buffer[38]);
      fe->compressed_size = getuint32((const unsigned char *)&buffer[16]);
      fe->size            = getuint32((const unsigned char *)&buffer[20]);
      fe->method          = getuint16((const unsigned char *)&buffer[6]);

      // LOG_D("File: %s %d %d %d %d", fe.filename, fe.start_pos, fe.compressed_size,
      // fe.size, fe.method);
      file_entries.push_front(fe);

      offset += FILE_ENTRY_SIZE + 4 + filename_size + extra_size + comment_size;
      if (fseek(file, extra_size + comment_size, SEEK_CUR)) break;
    }
  }
  if (!completed) {
    LOG_E("Unable to read central directory.");
  }

  return completed;
}

bool Unzip::open_zip_file(const char *zip_filename) {
  // Open zip file
  if (zip_file_is_open) close_zip_file();
  if ((file = fopen(zip_filename, "r")) == nullptr) {
    LOG_E("Unable to open file: %s", zip_filename);
    return false;
  }
  zip_file_is_open = true;

  bool completed = seek_to_central_directory() &&
                   read_file_entries(getuint32((const unsigned char *)&buffer[16]),
                                     getuint16((const unsigned char *)&buffer[10]));

  if (!completed) {
    close_zip_file();
  } else {
    LOG_D("open_zip_file completed!");
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto *f : file_entries) {
        std::cout << "pos: " << std::setw(7) << f->start_pos << " zip size: " << std::setw(7)
                  << f->compressed_size << " out size: " << std::setw(7) << f->size
                  << " method: " << std::setw(1) << f->method << " name: " << f->filename
                  << std::endl;
      }
      std::cout << "[End of List]" << std::endl;
    #endif
  }

  file_entries.reverse();
  return completed;
}

void Unzip::close_zip_file() {
  if (zip_file_is_open) {
    for (auto *entry : file_entries) {
      delete[] entry->filename;
      delete entry;
    }
    file_entries.clear();
    fclose(file);
    zip_file_is_open = false;
  }

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
char *clean_fname(const char *filename) {
  char *str = new char[strlen(filename) + 1];

  const char *s = filename;
  const char *u;
  char *t = str;

  while ((u = strstr(s, "/../")) != nullptr) {
    const char *ss = s;     // keep it for copy in target
    s              = u + 3; // prepare for next iteration
    do {                    // get rid of preceeding folder name
      u--;
    } while ((u > ss) && (*u != '/'));
    if (u >= ss) {
      while (ss != u) *t++ = *ss++;
    } else if ((*u != '/') && (t > str)) {
      do {
        t--;
      } while ((t > str) && (*t != '/'));
    }
  }
  if ((t == str) && (*s == '/')) s++;
  while ((*t++ = *s++));

  return str;
}

int32_t Unzip::get_file_size(const char *filename) {
  LOG_D("Mutex lock...");
  mutex.lock();

  if (!zip_file_is_open) return 0;

  char *the_filename = clean_fname(filename);
  current_fe         = file_entries.begin();

  while (current_fe != file_entries.end()) {
    if (strcmp((*current_fe)->filename, the_filename) == 0) break;
    current_fe++;
  }

  if (current_fe == file_entries.end()) {
    LOG_E("Unzip get_file_size: File not found: %s", the_filename);
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto *f : file_entries) {
        std::cout << "  <" << f->filename << ">" << std::endl;
      }
      std::cout << "[End of List]" << std::endl;
    #endif
    mutex.unlock();
    return 0;
  } else {
    mutex.unlock();
    return (*current_fe)->size;
  }
}

bool Unzip::file_exists(const char *filename) {
  if (!zip_file_is_open) return false;

  char *the_filename = clean_fname(filename);

  Unzip::FileEntries::iterator fe = file_entries.begin();

  while (fe != file_entries.end()) {
    if (strcmp((*fe)->filename, the_filename) == 0) break;
    fe++;
  }

  delete[] the_filename;

  return fe != file_entries.end();
}

bool Unzip::open_file(const char *filename) {
  LOG_D("Mutex lock...");
  mutex.lock();

  int err = 0;

  if (!zip_file_is_open) return false;

  char *the_filename = clean_fname(filename);
  current_fe         = file_entries.begin();

  while (current_fe != file_entries.end()) {
    if (strcmp((*current_fe)->filename, the_filename) == 0) break;
    current_fe++;
  }

  if (current_fe == file_entries.end()) {
    LOG_E("Unzip Get: File not found: %s", the_filename);
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto *f : file_entries) {
        std::cout << "  <" << f->filename << ">" << std::endl;
      }
      std::cout << "[End of List]" << std::endl;
    #endif
    mutex.unlock();
    return false;
  }
  // else {
  //   LOG_D("File: %s at pos: %d", (*fe)->filename, (*fe)->start_pos);
  // }

  delete[] the_filename;

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

  if ((fseek(file, (*current_fe)->start_pos, SEEK_SET) == 0) && (fread(buffer, 4, 1, file) == 1)) {

    if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 3) && (buffer[3] == 4))) {
      err = 15;
    } else if (fread(buffer, LOCAL_HEADER_SIZE, 1, file) != 1) {
      err = 16;
    } else {
      uint16_t filename_size = getuint16((const unsigned char *)&buffer[22]);
      uint16_t extra_size    = getuint16((const unsigned char *)&buffer[24]);

      if (fseek(file, filename_size + extra_size, SEEK_CUR)) {
        err = 17;
      } else {
        completed = true;
      }
    }
  } else {
    err = 13;
  }
  //   if (fread(buffer, 4, 1, file) != 1) ERR(14);
  //   if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 3) && (buffer[3] == 4)))
  //   ERR(15);

  //   if (fread(buffer, LOCAL_HEADER_SIZE, 1, file) != 1) ERR(16);

  //   uint16_t filename_size = getuint16((const unsigned char *)&buffer[22]);
  //   uint16_t extra_size    = getuint16((const unsigned char *)&buffer[24]);

  //   // bool has_data_descriptor = ((buffer[2] & 8) != 0);
  //   // if (has_data_descriptor) {
  //   //   LOG_D("Unzip: with data descriptor...");
  //   // }

  //   if (fseek(file, filename_size + extra_size, SEEK_CUR)) ERR(17);
  //   // LOG_D("Unzip Get Method: ", (*current_fe)->method);

  //   completed = true;
  // sortie:

  if (completed) {
    (*current_fe)->current_pos = 0;
    return true;
  } else {
    LOG_E("Unzip open_file: Error!: %d", err);
    LOG_D("Mutex unlock...");
    mutex.unlock();
    return false;
  }
}

void Unzip::close_file() {
  LOG_D("Mutex unlock...");
  mutex.unlock();
}

#if !STB

  bool Unzip::open_stream_file(const char *filename, uint32_t &file_size) {
    if (!open_file(filename)) return false;

    repeat  = ((*current_fe)->compressed_size) / BUFFER_SIZE;
    remains = ((*current_fe)->compressed_size) % BUFFER_SIZE;
    current = 0;
    aborted = false;

    zstr.zalloc    = nullptr;
    zstr.zfree     = nullptr;
    zstr.opaque    = nullptr;
    zstr.next_in   = nullptr;
    zstr.next_out  = nullptr;
    zstr.avail_in  = 0;
    zstr.avail_out = 0;

    int zret;

    if ((zret = mz_inflateInit2(&zstr, -15)) != MZ_OK) {
      aborted = true;
      close_stream_file();
      return false;
    }

    file_size = (*current_fe)->size;

    uint16_t size = current < repeat ? BUFFER_SIZE : remains;
    if (fread(buffer, size, 1, file) != 1) {
      LOG_E("Error reading zip content.");
      close_stream_file();
      aborted = true;
      return false;
    }

    current++;

    zstr.avail_in = size;
    zstr.next_in  = (unsigned char *)buffer;

    return true;
  }

  void Unzip::close_stream_file() {
    if (aborted) return;

    mz_inflateEnd(&zstr);

    close_file();
  }

  bool Unzip::stream_skip(uint32_t byte_count) {
    auto tmp      = make_unique_himem<char[]>(byte_count);
    uint32_t size = byte_count;

    do {
      uint32_t s = size;
      if ((s = get_stream_data(tmp.get(), s)) == 0) {
        return false;
      }
      size -= s;
    } while (size > 0);
    return true;
  }

  uint32_t Unzip::get_stream_data(char *data, uint32_t data_size) {

    if (data_size == 0) return 0;

    zstr.next_out  = (unsigned char *)data;
    zstr.avail_out = data_size;

    if ((*current_fe)->method == 0) {
      while (!aborted && (zstr.avail_out > 0)) {
        uint16_t copy_size = zstr.avail_in <= zstr.avail_out ? zstr.avail_in : zstr.avail_out;
        memcpy(zstr.next_out, zstr.next_in, copy_size);

        zstr.next_out += copy_size;
        zstr.next_in += copy_size;
        zstr.avail_out -= copy_size;
        zstr.avail_in -= copy_size;

        if (zstr.avail_in == 0) {
          if (current > repeat) break; // We are at the end
          uint16_t size = current < repeat ? BUFFER_SIZE : remains;
          if (fread(buffer, size, 1, file) != 1) {
            LOG_E("Error reading zip content.");
            close_stream_file();
            aborted = true;
            break;
          }

          current++;

          zstr.avail_in = size;
          zstr.next_in  = (unsigned char *)buffer;
        }
      }
    } else if ((*current_fe)->method == 8) {

      while (!aborted && (zstr.avail_out == data_size)) {

        int zret = mz_inflate(&zstr, MZ_NO_FLUSH);

        switch (zret) {
        case MZ_NEED_DICT:
        case MZ_DATA_ERROR:
        case MZ_MEM_ERROR:
          LOG_E("Error inflating data: %d", zret);
          close_stream_file();
          aborted = true;
        default:;
        }

        if (!aborted && (zstr.avail_out != 0)) {
          if ((zstr.avail_in == 0) && (current <= repeat)) {
            uint16_t size = current < repeat ? BUFFER_SIZE : remains;
            if (fread(buffer, size, 1, file) != 1) {
              LOG_E("Error reading zip content.");
              close_stream_file();
              aborted = true;
            } else {
              current++;

              zstr.avail_in = size;
              zstr.next_in  = (unsigned char *)buffer;
            }
          }
        }
      }
    }

    return !aborted ? data_size - zstr.avail_out : 0;
  }

  // Test version of get_file using stream methods
  auto Unzip::get_file(const char *filename, uint32_t &file_size) -> FileContentPtr {
    // LOG_D("get_file: %s", filename);

    uint32_t total = 0;

    bool completed     = false;
    bool stream_opened = false;

    FileContentPtr data{nullptr};

    if (!open_stream_file(filename, file_size)) {
      LOG_E("Unable to retrieve file %s", filename);
    } else {
      stream_opened = true;

      if ((data = make_unique_himem<uint8_t[]>(file_size + 1)) == nullptr) {
        LOG_E("Not enough memory to retrieve file %s", filename);
      } else {
        data[file_size] = 0;

        uint8_t *data_ptr = data.get();
        uint32_t size     = file_size;

        while (((size = get_stream_data((char *)data_ptr, size)) != 0) &&
               ((total + size) <= file_size)) {

          data_ptr += size;
          total += size;

          LOG_D("Got %" PRIu32 " bytes...", size);

          if (total == file_size) break;

          size = file_size - total;
        }

        LOG_D("File size: %" PRIu32 ", received: %" PRIu32, file_size, total);
        completed = true;
      }
    }

    if (stream_opened) close_stream_file();

    if (!completed) {
      file_size = 0;
      return nullptr;
    } else {
      file_size = total;
    }

    return data;
  }

#endif