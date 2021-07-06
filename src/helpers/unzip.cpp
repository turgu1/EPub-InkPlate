// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __UNZIP__ 1
#include "helpers/unzip.hpp"
#include "viewers/msg_viewer.hpp"
#include "logging.hpp"
#include "alloc.hpp"

#include "stb_image.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>

#define ZLIB 0

#if ZLIB
  #include "zlib.h" // ToDo: Migrate to stb
#endif

Unzip::Unzip()
{
  zip_file_is_open = false; 
}

bool 
Unzip::open_zip_file(const char * zip_filename)
{
  // Open zip file
  if (zip_file_is_open) close_zip_file();
  if ((file = fopen(zip_filename, "r")) == nullptr) {
    LOG_E("Unable to open file: %s", zip_filename);
    return false;
  }
  zip_file_is_open = true;

  int err = 0;

  #define ERR(e) { err = e; break; }

  bool completed = false;
  while (true) {
    // Seek to beginning of central directory
    //
    // We seek the file back until we reach the "End Of Central Directory"
    // signature "PK\5\6".
    //
    // end of central dir signature    4 bytes  (0x06054b50)
    // number of this disk             2 bytes   4
    // number of the disk with the
    // start of the central directory  2 bytes   6
    // total number of entries in the
    // central directory on this disk  2 bytes   8
    // total number of entries in
    // the central directory           2 bytes  10
    // size of the central directory   4 bytes  12
    // offset of start of central
    // directory with respect to
    // the starting disk number        4 bytes  16
    // .ZIP file comment length        2 bytes  20
    // --- SIZE UNTIL HERE: UNZIP_EOCD_SIZE ---
    // .ZIP file comment       (variable size)

    const int FILE_CENTRAL_SIZE = 22;

    buffer[FILE_CENTRAL_SIZE] = 0;

    if (fseek(file, 0, SEEK_END)) ERR(0);
    off_t length = ftell(file);
    if (length < FILE_CENTRAL_SIZE) ERR(1); 
    off_t offset = length - FILE_CENTRAL_SIZE;

    if (fseek(file, offset, SEEK_SET)) ERR(2);
    if (fread(buffer, FILE_CENTRAL_SIZE, 1, file) != 1) ERR(3);
    if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 5) && (buffer[3] == 6))) {
      // There must be a comment in the last entry. Search for the beginning of the entry
      offset -= FILE_CENTRAL_SIZE;
      bool found = false;
      while (!found && (offset > 0)) {
        if (fseek(file, offset, SEEK_SET)) ERR(4);
        if (fread(buffer, FILE_CENTRAL_SIZE, 1, file) != 1) ERR(5);
        char * p;
        if ((p = strstr(buffer, "PK\5\6")) != nullptr) {
          offset += (p - buffer);
          if (fseek(file, offset, SEEK_SET)) ERR(6);
          if (fread(buffer, FILE_CENTRAL_SIZE, 1, file) != 1) ERR(7);
          found = true;
          break;
        }
        offset -= FILE_CENTRAL_SIZE;
      }
      if (!found) offset = 0;
    }

    if (offset > 0) {
      offset         = getuint32((const unsigned char *) &buffer[16]);
      uint16_t count = getuint16((const unsigned char *) &buffer[10]);
  
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

      const int FILE_ENTRY_SIZE   = 42;

      if (count == 0) ERR(8);

      if (fseek(file, offset, SEEK_SET)) ERR(9);
      
      //file_entries.resize(count);

      while (true) {
        if (fread(buffer, 4, 1, file) != 1) ERR(10);
        if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 1) && (buffer[3] == 2))) {
          // End of list...
          completed = true;
          break;
        }
        if (fread(buffer, FILE_ENTRY_SIZE, 1, file) != 1) ERR(11);

        uint16_t filename_size = getuint16((const unsigned char *) &buffer[24]);
        uint16_t extra_size    = getuint16((const unsigned char *) &buffer[26]);
        uint16_t comment_size  = getuint16((const unsigned char *) &buffer[28]);
        
        char * fname = new char[filename_size + 1];
        if (fread(fname, filename_size, 1, file) != 1) {
          delete [] fname;
          break;
        }
        fname[filename_size] = 0;

        FileEntry * fe = new FileEntry;

        fe->filename        = fname;
        fe->start_pos       = getuint32((const unsigned char *) &buffer[38]);
        fe->compressed_size = getuint32((const unsigned char *) &buffer[16]);
        fe->size            = getuint32((const unsigned char *) &buffer[20]);
        fe->method          = getuint16((const unsigned char *) &buffer[ 6]);

        //LOG_D("File: %s %d %d %d %d", fe.filename, fe.start_pos, fe.compressed_size, fe.size, fe.method);
        file_entries.push_front(fe);

        offset += FILE_ENTRY_SIZE + 4 + filename_size + extra_size + comment_size;
        if (fseek(file, extra_size + comment_size, SEEK_CUR)) ERR(12);
      }
    }
    break;
  }

  if (!completed) {
    LOG_E("open_zip_file error: %d", err);
    close_zip_file();
  }
  else {
    // LOG_D("open_zip_file completed!");
  }

  file_entries.reverse();

  // for (auto * f : file_entries) {
  //   std::cout << 
  //     "filename: " << f->filename <<
  //     " start pos: " << f->start_pos <<
  //     " compressed size: " << f->compressed_size <<
  //     " size: " << f->size <<
  //     " method: " << f->method << std::endl;
  // }

  return completed;
}

void 
Unzip::close_zip_file()
{
  if (zip_file_is_open) {
    for (auto * entry : file_entries) {
      delete [] entry->filename;
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
char *
clean_fname(const char * filename)
{
  char * str = new char[strlen(filename) + 1];

  const char * s = filename;
  const char * u;
  char       * t = str;

  while ((u = strstr(s, "/../")) != nullptr) {
    const char * ss = s;   // keep it for copy in target
    s = u + 3;             // prepare for next iteration
    do {                   // get rid of preceeding folder name
      u--;
    } while ((u > ss) && (*u != '/'));
    if (u >= ss) {
      while (ss != u) *t++ = *ss++;
    }
    else if ((*u != '/') && (t > str)) {
      do {
        t--;
      } while ((t > str) && (*t != '/'));
    }
  }
  if ((t == str) && (*s == '/')) s++;
  while ((*t++ = *s++)) ;

  return str;
}

char * 
Unzip::get_file(const char * filename, uint32_t & file_size)
{
  // LOG_D("get_file: %s", filename);
  
  std::scoped_lock guard(mutex);
  
  char * data = nullptr;
  int    err  = 0;
  file_size = 0;
  
  if (!zip_file_is_open) return nullptr;

  char * the_filename = clean_fname(filename);
  FileEntries::iterator fe = file_entries.begin();

  while (fe != file_entries.end()) {
    if (strcmp((*fe)->filename, the_filename) == 0) break;
    fe++;
  }

  if (fe == file_entries.end()) {
    LOG_E("Unzip Get: File not found: %s", the_filename);
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto * f : file_entries) {
        std::cout << "  <" << f->filename << ">" << std::endl;
      }
      std::cout << "[End of List]" << std::endl;
    #endif
    return nullptr;
  }
  // else {
  //   LOG_D("File: %s at pos: %d", (*fe)->filename, (*fe)->start_pos);
  // }

  delete [] the_filename;

  bool completed = false;
  while (true) {

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

    if (fseek(file, (*fe)->start_pos, SEEK_SET)) ERR(13);
    if (fread(buffer, 4, 1, file) != 1) ERR(14);
    if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 3) && (buffer[3] == 4))) ERR(15);

    if (fread(buffer, LOCAL_HEADER_SIZE, 1, file) != 1) ERR(16);

    uint16_t filename_size = getuint16((const unsigned char *) &buffer[22]);
    uint16_t extra_size    = getuint16((const unsigned char *) &buffer[24]);

    // bool has_data_descriptor = ((buffer[2] & 8) != 0);
    // if (has_data_descriptor) {
    //   LOG_D("Unzip: with data descriptor...");
    // }

    if (fseek(file, filename_size + extra_size, SEEK_CUR)) ERR(17);
    // LOG_D("Unzip Get Method: ", fe->method);

    data = (char *) allocate((*fe)->size + 1);

    if (data == nullptr) ERR(18);
    data[(*fe)->size] = 0;

    #if SHOW_TIMING
      uint64_t a = 0, b = 0, c = 0;
    #endif

    if ((*fe)->method == 0) {

      #if SHOW_TIMING
        a = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
      #endif

      if (fread(data, (*fe)->size, 1, file) != 1) ERR(19);

      #if SHOW_TIMING
        b = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
      #endif

      //LOG_E("read %d bytes at pos %d", fe->size, (fe->start_pos + 4 + LOCAL_HEADER_SIZE + filename_size + extra_size));
    }
    else if ((*fe)->method == 8) {

      #if ZLIB
        if (result != fe->size) ERR(20);

        uint16_t rep   = fe->compressed_size / BUFFER_SIZE;
        uint16_t rem   = fe->compressed_size % BUFFER_SIZE;
        uint16_t cur   = 0;
        uint32_t total = 0;

        /* Allocate inflate state */
        z_stream zstr;

        zstr.zalloc    = nullptr;
        zstr.zfree     = nullptr;
        zstr.opaque    = nullptr;
        zstr.next_in   = nullptr;
        zstr.avail_in  = 0;
        zstr.avail_out = fe->size;
        zstr.next_out  = (Bytef *) data;

        int zret;

        // Use inflateInit2 with negative windowBits to get raw decompression
        if ((zret = inflateInit2_(&zstr, -MAX_WBITS, ZLIB_VERSION, sizeof(z_stream))) != Z_OK) goto error;

        // int szDecomp;

        // Decompress until deflate stream ends or end of file
        do
        {
          uint16_t size = cur < rep ? BUFFER_SIZE : rem;
          if (fread(buffer, size, 1, file) != 1) {
            inflateEnd(&zstr);
            goto error;
          }

          cur++;
          total += size;

          zstr.avail_in = size;
          zstr.next_in = (Bytef *) buffer;

          zret = inflate(&zstr, Z_NO_FLUSH);

          switch (zret) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
              inflateEnd(&zstr);
              goto error;
            default:
              ;
          }
        } while (zret != Z_STREAM_END);

        inflateEnd(&zstr);
      #else
        char * compressed_data = (char *) allocate((*fe)->compressed_size + 2);
        if (compressed_data == nullptr) {
          // msg_viewer.out_of_memory("compressed data retrieval from epub");
          ERR(21);
        }

        #if SHOW_TIMING
          a = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        #endif

        if (fread(compressed_data, (*fe)->compressed_size, 1, file) != 1) ERR(22);

        #if SHOW_TIMING
          b = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        #endif

        compressed_data[(*fe)->compressed_size]     = 0;
        compressed_data[(*fe)->compressed_size + 1] = 0;

        int32_t result = stbi_zlib_decode_noheader_buffer(data, (*fe)->size, compressed_data, (*fe)->compressed_size + 2);

        #if SHOW_TIMING
          c = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        #endif

        if (result != (*fe)->size) {
          ERR(23);
        }
        // std::cout << "[FILE CONTENT:]" << std::endl << data << std::endl << "[END]" << std::endl;

        free(compressed_data);
      #endif
    }
    else break;

    #if SHOW_TIMING
      std::cout << "unzip.get_file timings: get file: " << b - a;
      if (c != 0) std::cout << " unzip: " << c - b;
      std::cout << std::endl;
    #endif

    completed = true;
    break;
  }

  if (!completed) {
    if (data != nullptr) free(data);
    data = nullptr;
    file_size = 0;
    LOG_E("Unzip get: Error!: %d", err);
  }
  else {
    file_size = (*fe)->size;
  }
  return data;
}
