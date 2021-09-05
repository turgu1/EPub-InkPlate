// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include <forward_list>
#include <mutex>

#define MINIZ 1
#define ZLIB  0

#if MINIZ
  #include "miniz.h"
#else
  #include "zlib.h"
#endif

class Unzip
{
  private:
    static constexpr char const * TAG = "Unzip";

    static const int BUFFER_SIZE = 1024*16;
    char buffer[BUFFER_SIZE];

    std::mutex mutex;
    
    /**
     * @brief File descriptor inside the zip file
     * 
     */
    struct FileEntry {
      char * filename;
      uint32_t start_pos;       // in zip file
      uint32_t compressed_size; // in zip file
      uint32_t size;            // once decompressed
      uint32_t current_pos;
      uint16_t method;          // compress method (0 = not compressed, 8 = DEFLATE)
    };

    typedef std::forward_list<FileEntry *> FileEntries;
    FileEntries file_entries;
    FileEntries::iterator current_fe;

    uint32_t getuint32(const unsigned char * b) {
      return  ((uint32_t)b[0])        | 
             (((uint32_t)b[1]) <<  8) |
             (((uint32_t)b[2]) << 16) |
             (((uint32_t)b[3]) << 24) ;
    }
    uint16_t getuint16(const unsigned char * b) {
      return  ((uint32_t)b[0])      |     
             (((uint32_t)b[1]) << 8);
    }

    uint16_t repeat;
    uint16_t remains;
    uint16_t current;
    bool     aborted;

    FILE * file; // Current File Descriptor
    bool zip_file_is_open;

    #if ZLIB
      z_stream zstr;
    #endif
    #if MINIZ
      mz_stream zstr;
    #endif

  public:
    Unzip();
    bool open_zip_file(const char * zip_filename);
    void close_zip_file();
    
    char * get_file(const char * filename, uint32_t & file_size);
    bool   open_file(const char * filename);
    void   close_file();

    #if !STB
      bool   open_stream_file(const char * filename, uint32_t & file_size);
      bool   get_stream_data(char * data, uint32_t & size);
      bool   stream_skip(uint32_t byte_count);
      void   close_stream_file();
    #endif
};

#if __UNZIP__
  Unzip unzip;
#else
  extern Unzip unzip;
#endif

