#ifndef __UNZIP_HPP__
#define __UNZIP_HPP__

#include <vector>

#include "global.hpp"

class Unzip
{
  private:
    static const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    /**
     * @brief File descriptor inside the zip file
     * 
     */
    struct FileEntry {
      char * filename;
      uint32_t start_pos;       // in zip file
      uint32_t compressed_size; // in zip file
      uint32_t size;            // once decompressed
      uint16_t method;          // compress method (0 = not compressed, 8 = DEFLATE)
    };

    FILE * file; // Current File Descriptor
    bool zip_file_is_open;
    std::vector<FileEntry *> file_entries;

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

  public:
    Unzip();
    bool open_zip_file(const char * zip_filename);
    void close_zip_file();
    char * get_file(const char * filename, int & file_size);
};

#if __UNZIP__
  Unzip unzip;
#else
  extern Unzip unzip;
#endif

#endif
