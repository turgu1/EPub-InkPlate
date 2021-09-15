// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __UNZIP__ 1
#include "helpers/unzip.hpp"

#include "viewers/msg_viewer.hpp"
#include "alloc.hpp"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <cerrno>
#include <iomanip>

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

    if (fseek(file, 0, SEEK_END)) {
      LOG_E("Unable to seek to end of file. errno: %s", std::strerror(errno));
      err = 0;
      break;
    }
    off_t length = ftell(file);
    if (length < FILE_CENTRAL_SIZE) ERR(1); 
    off_t offset = length - FILE_CENTRAL_SIZE;

    if (fseek(file, offset, SEEK_SET)) ERR(2);
    if (fread(buffer, FILE_CENTRAL_SIZE, 1, file) != 1) ERR(3);
    if (!((buffer[0] == 'P') && (buffer[1] == 'K') && (buffer[2] == 5) && (buffer[3] == 6))) {
      // There must be a comment in the last entry. Search for the beginning of the entry
      off_t end_offset = offset - 65536;
      if (end_offset < 0) end_offset = 0;
      offset -= FILE_CENTRAL_SIZE;
      bool found = false;
      while (!found && (offset > end_offset)) {
        if (fseek(file, offset, SEEK_SET)) ERR(4);
        if (fread(buffer, FILE_CENTRAL_SIZE + 5, 1, file) != 1) ERR(5);
        char * p;
        if ((p = (char *)memmem(buffer, FILE_CENTRAL_SIZE + 5, "PK\5\6", 4)) != nullptr) {
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
          ERR(12);
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
        if (fseek(file, extra_size + comment_size, SEEK_CUR)) ERR(13);
      }
    }
    else {
      LOG_E("Unable to read central directory.");
      ERR(14);
    }
    break;
  }

  if (!completed) {
    LOG_E("open_zip_file error: %d", err);
    close_zip_file();
  }
  else {
    LOG_D("open_zip_file completed!");
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto * f : file_entries) {
        std::cout << 
          "pos: "        << std::setw(7) << f->start_pos <<
          " zip size: "  << std::setw(7) << f->compressed_size <<
          " out size: "  << std::setw(7) << f->size <<
          " method: "    << std::setw(1) << f->method <<
          " name: "      << f->filename <<  std::endl;
      }
     std::cout << "[End of List]" << std::endl;
    #endif
  }

  file_entries.reverse();
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

int32_t
Unzip::get_file_size(const char * filename)
{
  LOG_D("Mutex lock...");
  mutex.lock();
  
  if (!zip_file_is_open) return 0;

  char * the_filename = clean_fname(filename);
  current_fe = file_entries.begin();

  while (current_fe != file_entries.end()) {
    if (strcmp((*current_fe)->filename, the_filename) == 0) break;
    current_fe++;
  }

  if (current_fe == file_entries.end()) {
    LOG_E("Unzip get_file_size: File not found: %s", the_filename);
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto * f : file_entries) {
        std::cout << "  <" << f->filename << ">" << std::endl;
      }
      std::cout << "[End of List]" << std::endl;
    #endif    
    mutex.unlock();
    return 0;
  }
  else {
    mutex.unlock();
    return (*current_fe)->size;
  }
}

bool
Unzip::open_file(const char * filename)
{
  LOG_D("Mutex lock...");
  mutex.lock();
  
  int err = 0;

  if (!zip_file_is_open) return false;

  char * the_filename = clean_fname(filename);
  current_fe = file_entries.begin();

  while (current_fe != file_entries.end()) {
    if (strcmp((*current_fe)->filename, the_filename) == 0) break;
    current_fe++;
  }

  if (current_fe == file_entries.end()) {
    LOG_E("Unzip Get: File not found: %s", the_filename);
    #if DEBUGGING
      std::cout << "---- Files available: ----" << std::endl;
      for (auto * f : file_entries) {
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

    if (fseek(file, (*current_fe)->start_pos, SEEK_SET)) ERR(13);
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
    // LOG_D("Unzip Get Method: ", (*current_fe)->method);
    
    completed = true;
    break;
  }

  if (completed) {
    (*current_fe)->current_pos = 0;
    return true;
  }
  else {
    LOG_E("Unzip open_file: Error!: %d", err);
    LOG_D("Mutex unlock...");
    mutex.unlock();
    return false;
  }
}

void
Unzip::close_file()
{
  LOG_D("Mutex unlock...");
  mutex.unlock();
}

#if 0
char * 
Unzip::get_file(const char * filename, uint32_t & file_size)
{
  // LOG_D("get_file: %s", filename);
  
  if (!open_file(filename)) return nullptr;

  char * data = nullptr;
  int    err  = 0;
  file_size   = 0;
  
  bool completed = false;
  while (true) {
    data = (char *) allocate((*current_fe)->size + 1);

    if (data == nullptr) ERR(18);
    data[(*current_fe)->size] = 0;

    if ((*current_fe)->method == 0) {
      if (fread(data, (*current_fe)->size, 1, file) != 1) ERR(19);
    }
    else if ((*current_fe)->method == 8) {

      #if MINIZ
        repeat  = (*current_fe)->compressed_size / BUFFER_SIZE;
        remains = (*current_fe)->compressed_size % BUFFER_SIZE;
        current = 0;

        zstr.zalloc    = nullptr;
        zstr.zfree     = nullptr;
        zstr.opaque    = nullptr;
        zstr.next_in   = nullptr;
        zstr.avail_in  = 0;
        zstr.avail_out = (*current_fe)->size;
        zstr.next_out  = (unsigned char *) data;

        int zret;

        // Use inflateInit2 with negative windowBits to get raw decompression
        if ((zret = mz_inflateInit2(&zstr, -15)) != MZ_OK) {
          ERR(23);
        }

        // int szDecomp;

        // Decompress until deflate stream ends or end of file
        do {
          uint16_t size = current < repeat ? BUFFER_SIZE : remains;
          if (fread(buffer, size, 1, file) != 1) {
            mz_inflateEnd(&zstr);
            err = 24;
            goto error;
          }

          current++;

          zstr.avail_in = size;
          zstr.next_in  = (unsigned char *) buffer;

          zret = mz_inflate(&zstr, MZ_NO_FLUSH);

          switch (zret) {
            case MZ_NEED_DICT:
            case MZ_DATA_ERROR:
            case MZ_MEM_ERROR:
              mz_inflateEnd(&zstr);
              err = 25;
              goto error;
            default:
              ;
          }
        } while (current <= repeat);

        mz_inflateEnd(&zstr);
      #endif

      #if ZLIB
        repeat  = ((*current_fe)->compressed_size + 2) / BUFFER_SIZE;
        remains = ((*current_fe)->compressed_size + 2) % BUFFER_SIZE;
        current = 0;

        /* Allocate inflate state */
        z_stream zstr;

        zstr.zalloc    = nullptr;
        zstr.zfree     = nullptr;
        zstr.opaque    = nullptr;
        zstr.next_in   = nullptr;
        zstr.avail_in  = 0;
        zstr.avail_out = (*current_fe)->size;
        zstr.next_out  = (Bytef *) data;

        int zret;

        // Use inflateInit2 with negative windowBits to get raw decompression
        if ((zret = inflateInit2(&zstr, -MAX_WBITS)) != Z_OK) { 
          err = 23; 
          goto error; 
        }

        // int szDecomp;

        // Decompress until deflate stream ends or end of file
        do {
          uint16_t size = current < repeat ? BUFFER_SIZE : remains;
          if (fread(buffer, size, 1, file) != 1) {
            inflateEnd(&zstr);
            err = 24;
            goto error;
          }

          // if (size <= (BUFFER_SIZE - 2)) {
          //   buffer[size    ] = 0;
          //   buffer[size + 1] = 0;
          // }

          current++;

          zstr.avail_in = size;
          zstr.next_in  = (Bytef *) buffer;

          zret = inflate(&zstr, Z_NO_FLUSH);

          switch (zret) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
              inflateEnd(&zstr);
              err = 25;
              goto error;
            default:
              ;
          }
        } while (current <= repeat);

        inflateEnd(&zstr);
      #endif
      #if STB
        char * compressed_data = (char *) allocate((*current_fe)->compressed_size + 2);
        if (compressed_data == nullptr) {
          // msg_viewer.out_of_memory("compressed data retrieval from epub");
          ERR(21);
        }

        if (fread(compressed_data, (*current_fe)->compressed_size, 1, file) != 1) {
          free(compressed_data);
          ERR(22);
        }

        compressed_data[(*current_fe)->compressed_size]     = 0;
        compressed_data[(*current_fe)->compressed_size + 1] = 0;

        int32_t result = stbi_zlib_decode_noheader_buffer(data, 
                                                          (*current_fe)->size, 
                                                          compressed_data, 
                                                          (*current_fe)->compressed_size + 2);

        if (result != (*current_fe)->size) {
          free(compressed_data);
          ERR(23);
        }

        free(compressed_data);
      #endif
    }
    else break;

    completed = true;
    break;
  }

error:
  close_file();

  if (!completed) {
    if (data != nullptr) free(data);
    data = nullptr;
    file_size = 0;
    LOG_E("Unzip get: Error!: %d", err);
  }
  else {
    file_size = (*current_fe)->size;
  }

  return data;
}
#endif

#if !STB

bool
Unzip::open_stream_file(const char * filename, uint32_t & file_size)
{
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

  #if ZLIB
    if ((zret = inflateInit2(&zstr, -MAX_WBITS)) != Z_OK) {
      aborted = true;
      close_stream_file();
      return false;
    }
  #else // MINIZ
    if ((zret = mz_inflateInit2(&zstr, -15)) != MZ_OK) {
      aborted = true;
      close_stream_file();
      return false;
    }
  #endif

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
  zstr.next_in  = (unsigned char *) buffer;

  return true;
}

void
Unzip::close_stream_file() 
{
  if (aborted) return;

  #if ZLIB
    inflateEnd(&zstr);
  #else // MINIZ
    mz_inflateEnd(&zstr);
  #endif

  close_file();
}

bool
Unzip::stream_skip(uint32_t byte_count)
{
  char * tmp = (char *) allocate(byte_count);
  uint32_t size = byte_count;

  do {
    uint32_t s = size;
    if (!get_stream_data(tmp, s)) {
      free(tmp);
      return false;
    }
    size -= s;
  } while (size > 0);
  free(tmp);
  return true;
}

bool 
Unzip::get_stream_data(char * data, uint32_t & data_size)
{
  zstr.next_out  = (unsigned char *) data;
  zstr.avail_out = data_size;
  
  if ((*current_fe)->method == 0) {
    while (!aborted && (zstr.avail_out > 0)) {
      uint16_t copy_size = zstr.avail_in <= zstr.avail_out ? zstr.avail_in : zstr.avail_out;
      memcpy(zstr.next_out, zstr.next_in, copy_size);

      zstr.next_out  += copy_size;
      zstr.next_in   += copy_size;
      zstr.avail_out -= copy_size;
      zstr.avail_in  -= copy_size;

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
        zstr.next_in  = (unsigned char *) buffer;
      }

    }
  }
  else if ((*current_fe)->method == 8) {

    while (!aborted && (zstr.avail_out == data_size)) {

      #if ZLIB
        int zret = inflate(&zstr, Z_NO_FLUSH);

        switch (zret) {
          case Z_NEED_DICT:
          case Z_DATA_ERROR:
          case Z_MEM_ERROR:
            LOG_E("Error inflating data: %d", zret);
            close_stream_file();
            aborted = true;
          default:
            ;   
        }
      #else // MINIZ
        int zret = mz_inflate(&zstr, MZ_NO_FLUSH);

        switch (zret) {
          case MZ_NEED_DICT:
          case MZ_DATA_ERROR:
          case MZ_MEM_ERROR:
            LOG_E("Error inflating data: %d", zret);
            close_stream_file();
            aborted = true;
          default:
            ;   
        }
      #endif

      if (!aborted && (zstr.avail_out != 0)) {
        if ((zstr.avail_in == 0) && (current <= repeat)) {
          uint16_t size = current < repeat ? BUFFER_SIZE : remains;
          if (fread(buffer, size, 1, file) != 1) {
            LOG_E("Error reading zip content.");
            close_stream_file();
            aborted = true;
          }
          else {
            current++;

            zstr.avail_in = size;
            zstr.next_in  = (unsigned char *) buffer;
          }
        }
      }
    }
  }

  data_size = data_size - zstr.avail_out;
  return !aborted;
}

// Test version of get_file using stream methods
char * 
Unzip::get_file(const char * filename, uint32_t & file_size)
{
  // LOG_D("get_file: %s", filename);
  
  char * data   = nullptr;
  char * window = nullptr;
  int    total  = 0;
  int    err    = 0;
  
  bool completed = false;
  while (true) {
    if (!open_stream_file(filename, file_size)) ERR(18);

    if ((data = (char *) allocate(file_size + 1)) == nullptr) ERR(19);
    data[file_size] = 0;

    char   * data_ptr = data;
    uint32_t size     = file_size;

    while (get_stream_data(data_ptr, size) && ((total + size) <= file_size)) {
      if (size > 0) {
        data_ptr += size;
        total    += size;

        LOG_D("Got %d bytes...", size);
      }
      else {
        LOG_E("Got 0 bytes!!");
      }

      if (total == file_size) break;

      size = file_size - total;
    }

    LOG_D("File size: %d, received: %d", file_size, total);
    free(window);
    completed = true;
    break;
  }

  close_stream_file();

  if (!completed) {
    if (data   != nullptr) free(data);
    if (window != nullptr) free(window);
    data = nullptr;
    file_size = 0;
    LOG_E("Unzip get (stream version): Error!: %d", err);
  }
  else {
    file_size = total;
  }

  return data;
}

#endif