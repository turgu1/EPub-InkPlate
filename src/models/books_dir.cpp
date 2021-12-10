// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOKS_DIR__ 1
#include "models/books_dir.hpp"

#include "models/epub.hpp"
#include "models/default_cover.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "alloc.hpp"

#if EPUB_INKPLATE_BUILD
  #include "models/nvs_mgr.hpp"
  #include "esp.hpp"
#endif

extern "C" { 
  #include <dirent.h>
}

#include <sys/stat.h>
#include <stdlib.h>
#include <sstream>

#if 0
  const uint32_t CRC32_INITIAL    = 0xFFFFFFFFUL;
  const uint32_t CRC32_POLYNOMIAL = 0x1EDC6F41UL;

  static uint32_t 
  generate_id(const uint8_t * buffer, uint32_t bufferLength)
  {
    uint32_t i;
    int8_t   j;
    uint32_t mask;
    uint32_t crc = CRC32_INITIAL;

    for (i = 0; i < bufferLength; i++) {
      crc ^= ((uint8_t *)buffer)[i];
      for (j = 7; j >= 0; j--) {
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (CRC32_POLYNOMIAL & mask);
      }
    }

    return crc;
  }
#else

  // Jenkins96 algorithm. See: http://burtleburtle.net/bob/hash/evahash.html

  #define mix(a, b, c) \
  { \
    a = a - b;  a = a - c;  a = a ^ (c >> 13); \
    b = b - c;  b = b - a;  b = b ^ (a <<  8); \
    c = c - a;  c = c - b;  c = c ^ (b >> 13); \
    a = a - b;  a = a - c;  a = a ^ (c >> 12); \
    b = b - c;  b = b - a;  b = b ^ (a << 16); \
    c = c - a;  c = c - b;  c = c ^ (b >>  5); \
    a = a - b;  a = a - c;  a = a ^ (c >>  3); \
    b = b - c;  b = b - a;  b = b ^ (a << 10); \
    c = c - a;  c = c - b;  c = c ^ (b >> 15); \
  }

  uint32_t generate_id(const uint8_t * k, uint32_t bufferLength)
  {
    uint32_t a, b, c;
    uint32_t len;

    len = bufferLength;
    a = b = 0x9e3779b9;
    c = 0;

    // handle most of the key 
    while (len >= 12)
    {
        a = a + *((uint32_t *) &k[0]); //(k[0] + ((uint32_t)k[1] << 8) + ((uint32_t)k[ 2] << 16) + ((uint32_t)k[ 3] << 24));
        b = b + *((uint32_t *) &k[4]); //(k[4] + ((uint32_t)k[5] << 8) + ((uint32_t)k[ 6] << 16) + ((uint32_t)k[ 7] << 24));
        c = c + *((uint32_t *) &k[8]); //(k[8] + ((uint32_t)k[9] << 8) + ((uint32_t)k[10] << 16) + ((uint32_t)k[11] << 24));
        mix(a, b, c);
        k = k + 12; len -= 12;
    }

    /*------------------------------------- handle the last 11 bytes */
    c = c + bufferLength;
    switch (len) {
      case 11: c = c + ((uint32_t)k[10] << 24); [[fallthrough]];
      case 10: c = c + ((uint32_t)k[ 9] << 16); [[fallthrough]];
      case 9 : c = c + ((uint32_t)k[ 8] <<  8); [[fallthrough]];
          /* the first byte of c is reserved for the length */
      case 8 : b = b + ((uint32_t)k[ 7] << 24); [[fallthrough]];
      case 7 : b = b + ((uint32_t)k[ 6] << 16); [[fallthrough]];
      case 6 : b = b + ((uint32_t)k[ 5] <<  8); [[fallthrough]];
      case 5 : b = b + k[4];                    [[fallthrough]];
      case 4 : a = a + ((uint32_t)k[ 3] << 24); [[fallthrough]];
      case 3 : a = a + ((uint32_t)k[ 2] << 16); [[fallthrough]];
      case 2 : a = a + ((uint32_t)k[ 1] <<  8); [[fallthrough]];
      case 1 : a = a + k[0];
      /* case 0: nothing left to add */
    }
    mix(a, b, c);

    return c;
  }

#endif

bool 
BooksDir::read_books_directory(char * book_filename, int16_t & book_index)
{
  LOG_D("Reading books directory: %s.", BOOKS_DIR_FILE);

  if (!db.open(BOOKS_DIR_FILE)) {
    LOG_E("Can't open database: %s", BOOKS_DIR_FILE);
    return false;
  }

  // #if DEBUGGING
  //   show_db();
  // #endif

  // We first verify if the database content is of the current version

  bool version_ok = false;
  VersionRecord version_record;

  if (db.get_record_count() == 0) {
    memset(&version_record, 0, sizeof(version_record));
  
    version_record.version = BOOKS_DIR_DB_VERSION;
    strcpy(version_record.app_name, APP_NAME);

    if (!db.add_record(&version_record, sizeof(version_record))) {
      LOG_E("Not able to set DB Version.");
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

    LOG_I("Database is of a wrong version or doesn't exists. Initializing...");

    if (!db.create(BOOKS_DIR_FILE)) {
      LOG_E("Unable to create database: %s", BOOKS_DIR_FILE);
      return false;
    }

    memset(&version_record, 0, sizeof(version_record));
    version_record.version = BOOKS_DIR_DB_VERSION;
    strcpy(version_record.app_name, APP_NAME);

    if (!db.add_record(&version_record, sizeof(version_record))) {
      LOG_E("Not able to set DB Version.");
      return false;
    }
  }

  if (!refresh(book_filename, book_index)) {
    LOG_E("Unable to complete DB refresh");
    return false;
  }

  //show_db();

  LOG_D("Reading directory completed.");
  return true;
}

#if 0 // no more required
template<typename POD>
std::ostream & serialize(std::ostream & os, std::vector<POD> const & v)
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
std::istream & deserialize(std::istream & is, std::vector<POD> & v)
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
#endif

const BooksDir::EBookRecord * 
BooksDir::get_book_data(uint16_t idx)
{
  if (idx >= sorted_index.size()) {
    LOG_E("Idx too large: %d", idx);
    return nullptr;
  }

  int i = 0;
  int16_t index = -1;

  for (auto & entry : sorted_index) {
    if (idx == i) { index = entry.second.db_index; break; }
    i++;
  }
  if (index == -1) {
    LOG_E("Unable to find idx: %d", idx);
    return nullptr;
  }

  db.set_current_idx(index);

  if (!db.get_record(&book, sizeof(EBookRecord))) {
    LOG_E("Unable to get record at index %d", index);
    return nullptr;
  }

  current_book_idx = idx;

  return &book;
}
 
bool
BooksDir::get_book_id(uint16_t idx, uint32_t & id)
{
  if (idx >= sorted_index.size()) {
    LOG_E("Idx too large: %d", idx);
    return false;
  }

  int i = 0;
  bool found = false;

  for (auto & entry : sorted_index) {
    if (idx == i) { id = entry.second.id; found = true; break; }
    i++;
  }
  if (!found) LOG_E("Unable to find idx: %d", idx);

  return found;
}

bool
BooksDir::get_book_index(uint32_t id, uint16_t & idx)
{
  int  i     = 0;
  bool found = false;

  for (auto & entry : sorted_index) {
    if (entry.second.id == id) { idx = i; found = true; break; }
    i++;
  }
  if (!found) LOG_E("Unable to find id: 0x%08x", id);

  return found;
}

void
BooksDir::set_track_order(uint32_t id, int8_t pos)
{
  static bool no_recurse = false;
  if (no_recurse) return;

  LOG_D("-------------------------> set_track_order(%u, %d)", id, pos);
  bool found = false;

  for (auto & entry : sorted_index) {
    if (entry.second.id == id) {
      char ch = (pos >= 0) ? 'a' + pos : 'z';
      LOG_D("Old key: %s", entry.first.c_str());
      if (entry.first.front() != ch) {
        auto e = sorted_index.extract(entry.first);
        e.key().front() = ch;
        LOG_D("New key: %s", e.key().c_str());
        sorted_index.insert(std::move(e));
      }
      found = true;
      break;
    }
  }

  #if EPUB_INKPLATE_BUILD
    if (!found) {
      no_recurse = true;
      nvs_mgr.erase(id);
      no_recurse = false;
    }
  #endif
}

const BooksDir::EBookRecord * 
BooksDir::get_book_data_from_db_index(uint16_t idx)
{
  db.set_current_idx(idx);

  if (!db.get_record(&book, sizeof(EBookRecord))) {
    LOG_E("Unable to get record for db index %d", idx);
    return nullptr;
  }

  current_book_idx = idx;

  return &book;
}

bool
BooksDir::refresh(char * book_filename, int16_t & book_index, bool force_init)
{
  //  First look if existing entries in the database exists as ebook.
  //  Build a list of filenames for next step.

  LOG_D("Refreshing database content");

  EBookRecord   * the_book = nullptr;
  struct dirent * de       = nullptr;
  DIR           * dp       = nullptr;
  bool            first    = true;

  SortedIndex     temp_index;

  bool some_added_record = false;

  sorted_index.clear();

  if (force_init) {
    // Remove all records
    db.goto_first();
    while (db.goto_next()) {
      db.set_deleted();
    }
  }
  else {
    struct PartialRecord {
      char     filename[FILENAME_SIZE];
      int32_t  file_size;
      uint32_t id;
      char     title[TITLE_SIZE];
    } * partial_record = (PartialRecord *) allocate(sizeof(PartialRecord));

    if (partial_record == nullptr) msg_viewer.out_of_memory("partial record allocation");
    
    db.goto_first(); // Go pass the DB version record

    while (db.goto_next()) {
      db.get_record(partial_record, sizeof(PartialRecord));

      std::string fname = BOOKS_FOLDER "/";
      fname.append(partial_record->filename);

      struct stat stat_buffer;   

      // if file with filename not found or the file size is not the same, 
      // remove the database entry
      if ((stat(fname.c_str(), &stat_buffer) != 0) || 
          (stat_buffer.st_size != partial_record->file_size)) {
        LOG_D("Book no longer available: %s", partial_record->filename);
        db.set_deleted();
      }
      else {
        LOG_D("Title: %s", partial_record->title);
        temp_index[partial_record->filename] = IndexInfo { 
          .id = 0, 
          .db_index = 0 }; 

        #if EPUB_INKPLATE_BUILD
          int8_t pos = nvs_mgr.get_pos(partial_record->id);
          std::string title = " ";
          title += partial_record->title;
          title.front() = (pos >= 0) ? 'a' + pos : 'z';
        #else
          std::string title = "z";
          title += partial_record->title;
        #endif

        sorted_index[title] = IndexInfo {
          .id      = partial_record->id,
          .db_index = db.get_current_idx() };
        if (book_filename) {
          if (strcmp(book_filename, partial_record->filename) == 0) book_index = db.get_current_idx();
        }
      }
    }
    
    free(partial_record);
  }

  if (db.is_some_record_deleted()) {

    // Some record have been deleted. We have to recreate a database
    // with the cleaned records

    SimpleDB * new_db = new SimpleDB;
    sorted_index.clear();

    if (new_db->create(NEW_DIR_FILE)) {
      if (!db.goto_first()) {
        LOG_E("db.goto_first() failed");
        goto error_clear;
      }
      bool first = true;
      do {
        int32_t size = db.get_record_size();
        if (size <= 0) {
          LOG_E("Unable to get proper record size: %d from db", size);
          goto error_clear;
        }
        EBookRecord * data = (EBookRecord *) allocate(size);
        if (data == nullptr) {
          LOG_E("Unable to allocate %d bytes for ebook record", size);
          goto error_clear;
        }
        if (!db.get_record(data, size)) { 
          LOG_E("Unable to get record of size %d from db", size);
          free(data); 
          goto error_clear; 
        }
        if (!new_db->add_record(data, size)) {
          LOG_E("Unable to add record to db");
          free(data); 
          goto error_clear; 
        }
        if (!first) {
          uint16_t idx = new_db->get_record_count() - 1;
          #if EPUB_INKPLATE_BUILD
            int8_t pos = nvs_mgr.get_pos(data->id);
            std::string title = " ";
            title += data->title;
            title.front() = (pos >= 0) ? 'a' + pos : 'z';
          #else
            std::string title = "z";
            title += data->title;
          #endif
          sorted_index[title] = IndexInfo {
            .id       = data->id,
            .db_index = idx };
          if (book_filename) {
            if (strcmp(book_filename, data->filename) == 0) book_index = new_db->get_record_count() - 1;
          }
        }
        first = false;
        free(data);
      } while (db.goto_next());

      db.close();
      new_db->close();

      delete new_db;
      if (remove(BOOKS_DIR_FILE)) {
        LOG_E("Unable to remove directory DB file."); 
        goto error_clear;
      }
      if (rename(NEW_DIR_FILE, BOOKS_DIR_FILE)) {
        LOG_E("Unable to rename new directory DB file");
        goto error_clear;
      }
      if (!db.open(BOOKS_DIR_FILE)) {
        LOG_E("Inable to open directory DB File.");
        goto error_clear;
      }
    }
  }

  // Find ebooks that are new since last database refresh

  LOG_D("Looking at book files in folder %s", BOOKS_FOLDER);
 
  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif
  
  dp = opendir(BOOKS_FOLDER);

  if (dp != nullptr) {

    while ((de = readdir(dp))) {

      int16_t size = strlen(de->d_name);
      if ((size > 5) && (strcasecmp(&de->d_name[size - 5], ".epub") == 0)) {

        std::string fname = de->d_name;

        // check if ebook file named fname is in the database

        if (temp_index.find(fname) == temp_index.end()) {

          // The book is not in the database, we add it now

          if (first) {
            first = false;
            //msg_viewer.show_progress("Computing new books pages location...");
            if (force_init) {
              msg_viewer.show(MsgViewer::INFO, false, true, 
                "E-books metadata retrieval", 
                "System parameters changed requiring metadata retrieval. "
                "It will take between 5 and 10 seconds for each book.");
            }
            else {
              msg_viewer.show(MsgViewer::INFO, false, true, 
                "New e-books metadata retrieval", 
                "New e-books have been found. Please wait while we retrieve some metadata. "
                "It will take between 5 and 10 seconds for each e-book.");
            }
          }
          some_added_record = true;
          
          LOG_D("New book found: %s", de->d_name);

          fname = BOOKS_FOLDER "/";
          fname.append(de->d_name);

          int32_t file_size = 0;
          struct  stat stat_buffer;
          if (stat(fname.c_str(), &stat_buffer) != 0) {
            LOG_E("Unable to get stats for file: %s", fname.c_str());
            goto error_clear;
          }
          else {
            file_size = stat_buffer.st_size;
          }
        
          LOG_D("Opening file through the EPub class: %s", fname.c_str());

          if (epub.open_file(fname)) {
            const char * str;

            the_book = (EBookRecord *) allocate(sizeof(EBookRecord));
            
            if (the_book == nullptr) {
              LOG_E("Not enough memory for new book: %d bytes required.", sizeof(EBookRecord));
              goto error_clear;
            }

            memset(the_book, 0, sizeof(EBookRecord));

            LOG_D("Retrieving metadata and cover");
            strlcpy(the_book->filename, de->d_name, FILENAME_SIZE);
            the_book->file_size = file_size;
            the_book->id        = generate_id((uint8_t *)the_book->filename, strlen(the_book->filename));

            if ((str =       epub.get_title())) strlcpy(the_book->title,       str, TITLE_SIZE      );
            if ((str =      epub.get_author())) strlcpy(the_book->author,      str, AUTHOR_SIZE     );
            if ((str = epub.get_description())) strlcpy(the_book->description, str, DESCRIPTION_SIZE);

            std::string filename = epub.get_cover_filename();

            if (!filename.empty()) {

              // LOG_D("Cover filename: %s", filename);
              Image * img = epub.get_image(filename, true);
              if (img == nullptr) {
                LOG_D("Unable to retrieve cover file: %s", filename.c_str());
                memcpy(the_book->cover_bitmap, default_cover, default_cover_width * default_cover_height);
                the_book->cover_width     = default_cover_width;
                the_book->cover_height    = default_cover_height;
              }
              else {
                LOG_D("Image: width: %d height: %d", img->get_dim().width, img->get_dim().height);

                int32_t w = max_cover_width;
                int32_t h = img->get_dim().height * max_cover_width / img->get_dim().width;

                if (h > max_cover_height) {
                  h = max_cover_height;
                  w = img->get_dim().width * max_cover_height / img->get_dim().height;
                }

                img->resize(Dim(w, h));
                memcpy(the_book->cover_bitmap, img->get_bitmap(), w * h);

                the_book->cover_width     = w;
                the_book->cover_height    = h;

                delete img;
              }
            }
            else {
              memcpy(the_book->cover_bitmap, default_cover, default_cover_width * default_cover_height);
              the_book->cover_width     = default_cover_width;
              the_book->cover_height    = default_cover_height;
            }
        
            if (!db.add_record(the_book, sizeof(EBookRecord))) {
              LOG_E("Unable to add a new record to DB file.");
              goto error_clear;
            }

            uint16_t idx = db.get_record_count() - 1;
            #if EPUB_INKPLATE_BUILD
              int8_t pos = nvs_mgr.get_pos(the_book->id);
              std::string title = " ";
              title += the_book->title;
              title.front() = (pos >= 0) ? 'a' + pos : 'z';
            #else
              std::string title = "z";
              title += the_book->title;
            #endif
            sorted_index[title] = {
              .id       = the_book->id,
              .db_index = idx };

            if (book_filename) {
              if (strcmp(book_filename, the_book->filename) == 0) book_index = db.get_record_count() - 1;
            }

            epub.close_file();
            free(the_book);

            the_book = nullptr;

            #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
              ESP::show_heaps_info();
            #endif
          }
        }
      }
    }

    if (the_book) free(the_book);
    closedir(dp);
  }

  temp_index.clear();
  if (some_added_record) {
    db.close(); // To ensure that data is well written on SD Card
    if (!db.open(BOOKS_DIR_FILE)) {
       LOG_E("Unable to open db file");
       return false;
    }
  }

  return true;

error_clear:
  temp_index.clear();
  if (dp) closedir(dp);
  if (the_book) free(the_book);
  return false;
}

void
BooksDir::show_db()
{
  #if DEBUGGING
    VersionRecord  version_record;
    EBookRecord    book;

    if (!db.goto_first()) return;
    
    if (!db.get_record(&version_record, sizeof(VersionRecord))) return;

    std::cout << 
      "DB Version: "    << version_record.version  << 
      " app: "          << version_record.app_name << 
      " record count: " << db.get_record_count() - 1 << std::endl;

    while (db.goto_next()) {
      if (!db.get_record(&book, sizeof(EBookRecord))) return;
      std::cout 
        << "Book: "          << book.filename        << std::endl
        << "  id: "          << book.id              << std::endl
        << "  title: "       << book.title           << std::endl
        << "  author: "      << book.author          << std::endl
        << "  description: " << book.description     << std::endl
        << "  bitmap size: " << +book.cover_width 
        << " "               << +book.cover_height   << std::endl;
    }
  #endif
}