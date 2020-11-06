#define __BOOKS_DIR__ 1
#include "books_dir.hpp"

#include "epub.hpp"
#include "book_view.hpp"
#include "logging.hpp"
#include "alloc.hpp"

#include "stb_image.h"
#include "stb_image_resize.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sstream>

static const char * TAG = "BooksDir";

static const char * BOOKS_DIR_FILE = BOOKS_FOLDER "/books_dir.db";
static const char * NEW_DIR_FILE   = BOOKS_FOLDER "/new_dir.db";

bool 
BooksDir::read_books_directory()
{
  if (!db.open(BOOKS_DIR_FILE)) {
    LOG_E(TAG, "Can't open database: %s", BOOKS_DIR_FILE);
    return false;
  }

  // We first verify if the database content is of the current version

  bool version_ok = false;

  struct VersionRecord {
    uint16_t version;
    char     app_name[32];
  } version_record;

  if (db.get_record_count() == 0) {
    version_record.version = BOOKS_DIR_DB_VERSION;
    strcpy(version_record.app_name, APP_NAME);
    if (!db.add_record(&version_record, sizeof(version_record))) {
      LOG_E(TAG, "Not able to set DB Version.");
      return false;
    }
    version_ok = true;
  }
  else {
    db.goto_first();
    if (db.get_record_size() == sizeof(version_record)) {
      db.get_record(&version_record, sizeof(version_record));
      if ((version_record.version == BOOKS_DIR_DB_VERSION) &&
          (strcmp(version_record.app_name, APP_NAME) == 0)) {
        version_ok = true;
      }
    }
  }

  if (!version_ok) {

    LOG_I(TAG, "Database is of a wrong version or doesn't exists. Initializing...");

    if (!db.create(BOOKS_DIR_FILE)) {
      LOG_E(TAG, "Unable to create database: %s", BOOKS_DIR_FILE);
      return false;
    }

    version_record.version = BOOKS_DIR_DB_VERSION;
    strcpy(version_record.app_name, APP_NAME);

    if (!db.add_record(&version_record, sizeof(version_record))) {
      LOG_E(TAG, "Not able to set DB Version.");
      return false;
    }
  }

  refresh();

  sorted_index.clear();

  char * filename = (char *) malloc(FILENAME_SIZE);

  db.goto_first();  // First is database version... skip

  while (db.goto_next()) {
    db.get_record(filename, FILENAME_SIZE);
    sorted_index[filename] = db.get_current_idx();
  }

  return true;
}

template<typename POD>
std::ostream& serialize(std::ostream& os, std::vector<POD> const& v)
{
    // this only works on built in data types (PODs)
    static_assert(std::is_trivial<POD>::value && std::is_standard_layout<POD>::value,
        "Can only serialize POD types with this function");

    int32_t size = v.size();
    os.write(reinterpret_cast<char const *>(&size), sizeof(size));
    os.write(reinterpret_cast<char const *>(v.data()), v.size() * sizeof(POD));
    return os;
}

template<typename POD>
std::istream& deserialize(std::istream& is, std::vector<POD>& v)
{
    static_assert(std::is_trivial<POD>::value && std::is_standard_layout<POD>::value,
        "Can only deserialize POD types with this function");

    int32_t size;
    is.read(reinterpret_cast<char *>(&size), sizeof(size));
    v.resize(size);
    // std::cout << "Size: " << size << std::endl;
    is.read(reinterpret_cast<char *>(v.data()), v.size() * sizeof(POD));
    return is;
}

bool 
BooksDir::get_page_locs(EPub::PageLocs & page_locs, int16_t idx)
{
  page_locs.clear();

  if (idx >= sorted_index.size()) return false;

  int i = 0;
  int16_t index = -1;

  for (auto & entry : sorted_index) {
    if (idx == i) { index = entry.second; break; }
    i++;
  }
  if (index == -1) return false;

  db.set_current_idx(index);

  int32_t size = db.get_record_size() - sizeof(EBookRecord);

  if (size <= 0) return false;

  uint8_t * data = (uint8_t *) malloc(size);

  std::stringstream str;
  
  if (!db.get_partial_record(data, size, sizeof(EBookRecord))) return false;

  str.write((const char *) data, size);

  deserialize(str, page_locs);

  return true;
}

const BooksDir::EBookRecord * 
BooksDir::get_book_data(uint16_t idx)
{
  if (idx >= sorted_index.size()) return nullptr;

  int i = 0;
  int16_t index = -1;

  for (auto & entry : sorted_index) {
    if (idx == i) { index = entry.second; break; }
    i++;
  }
  if (index == -1) return nullptr;

  db.set_current_idx(index);

  if (!db.get_record(&book, sizeof(EBookRecord))) return nullptr;

  current_book_idx = idx;

  return &book;
}

bool
BooksDir::refresh()
{
  //  First look if existing entries in the database exists as ebook.

  struct PartialRecord {
    char filename[FILENAME_SIZE];
    int32_t file_size;
  } partial_record;

  db.goto_first();
  while (db.goto_next()) {
    db.get_record(&partial_record, sizeof(partial_record));

    std::string fname = BOOKS_FOLDER;
    fname.append(partial_record.filename);

    struct stat stat_buffer;   
    if ((stat(fname.c_str(), &stat_buffer) != 0) || 
        (stat_buffer.st_size != partial_record.file_size)) {
      db.set_deleted();
    } 
  }
  
  if (db.is_some_record_deleted()) {

    SimpleDB * new_db = new SimpleDB;
    sorted_index.clear();

    if (new_db->create(NEW_DIR_FILE)) {
      db.goto_first();
      bool first = true;
      do {
        int32_t size = db.get_record_size();
        uint8_t * data = (uint8_t *) malloc(size);
        if (!db.get_record(data, size)) return false;
        if (!new_db->add_record(data, size)) return false;
        if (!first) {
          sorted_index[(char *) data] = new_db->get_record_count() - 1;
        }
        first = false;
        free(data);
      } while (db.goto_next());

      db.close();
      new_db->close();

      delete new_db;
      if (remove(BOOKS_DIR_FILE)) return false;
      if (rename(NEW_DIR_FILE, BOOKS_DIR_FILE)) return false;
      if (!db.open(BOOKS_DIR_FILE)) return false;
    }
  }

  // Find ebooks that are new since last database refresh

  struct dirent *de = nullptr;
  DIR *dp = nullptr;

  dp = opendir(BOOKS_FOLDER);
  if (dp != nullptr) {
    while ((de = readdir(dp))) {
      int16_t size = strlen(de->d_name);
      if ((size > 5) && (strcasecmp(&de->d_name[size - 5], ".epub") == 0)) {

        std::string fname = de->d_name;

        // check if ebook file named fname is in the database

        if (sorted_index.find(fname) == sorted_index.end()) {

          // The book is not in the database, we add it now
          fname = BOOKS_FOLDER;
          fname.append(de->d_name);

          int32_t file_size = 0;
          struct  stat stat_buffer;
          if (stat(fname.c_str(), &stat_buffer) != 0) {
            LOG_E(TAG, "Unable to get stats for file: %s", fname.c_str());
          }
          else {
            file_size = stat_buffer.st_size;
          }
        
          if (epub.open_file(fname)) {
            const char * str;

            book_view.build_page_locs(); // This build epub::page_locs vector
            std::stringstream streamed_page_locs;

            serialize(streamed_page_locs, epub.get_page_locs());

            int32_t record_size = sizeof(EBookRecord) + streamed_page_locs.str().size() + 1;
            EBookRecord * the_book = (EBookRecord *) allocate(record_size);
            
            memcpy(
              the_book->pages_data, 
              streamed_page_locs.str().c_str(), 
              streamed_page_locs.str().size());

            strlcpy(the_book->filename, de->d_name, FILENAME_SIZE);
            the_book->file_size    = file_size;

            if ((str =       epub.get_title())) strlcpy(the_book->title,       str, TITLE_SIZE);
            if ((str =      epub.get_author())) strlcpy(the_book->author,      str, AUTHOR_SIZE);
            if ((str = epub.get_description())) strlcpy(the_book->description, str, DESCRIPTION_SIZE);

            const char * filename = epub.get_cover_filename();

            if (filename != nullptr) {

              // LOG_D(TAG, "Cover filename: %s", filename);

              int16_t channel_count;
              Page::Image image;

              std::string fname = filename;
              if (!epub.get_image(fname, image, channel_count)) {
                LOG_E(TAG, "Unable to retrieve cover file: %s", filename);
              }
              else {

                LOG_D(TAG, "Image: width: %d height: %d channel_count: %d", 
                  image.width, image.height, channel_count);

                int32_t w = max_cover_width;
                int32_t h = image.height * max_cover_width / image.width;
                if (h > max_cover_height) {
                  h = max_cover_height;
                  w = image.width * max_cover_height / image.height;
                }

                stbir_resize_uint8(image.bitmap, image.width, image.height, 0,
                                  (unsigned char *) (the_book->cover_bitmap), w, h, 0,
                                  1);

                the_book->cover_width  = w;
                the_book->cover_height = h;
                stbi_image_free((void *) image.bitmap);
              }
            }
        
            db.add_record(the_book, record_size);

            epub.close_file();
          }
        }
      }
    }
  }

  closedir(dp);

  return true;
}
