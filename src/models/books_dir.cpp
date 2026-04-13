// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// #undef DEBUGGING
// #define DEBUGGING 1

#define __BOOKS_DIR__ 1
#include "models/books_dir.hpp"

#include "models/config.hpp"
#include "models/default_cover.hpp"
#include "models/epub.hpp"
#include "picture_factory.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE_BUILD
  #include "esp.hpp"
  #include "models/nvs_mgr.hpp"
#endif

extern "C" {
#include <dirent.h>
}

#include "books_dir.hpp"
#include <sstream>
#include <stdlib.h>
#include <sys/stat.h>

Dim BooksDir::cover_dim{BooksDir::SMALL_COVER_WIDTH, BooksDir::SMALL_COVER_HEIGHT};

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

  #define mix(a, b, c)                                                                             \
    {                                                                                              \
      a = a - b;                                                                                   \
      a = a - c;                                                                                   \
      a = a ^ (c >> 13);                                                                           \
      b = b - c;                                                                                   \
      b = b - a;                                                                                   \
      b = b ^ (a << 8);                                                                            \
      c = c - a;                                                                                   \
      c = c - b;                                                                                   \
      c = c ^ (b >> 13);                                                                           \
      a = a - b;                                                                                   \
      a = a - c;                                                                                   \
      a = a ^ (c >> 12);                                                                           \
      b = b - c;                                                                                   \
      b = b - a;                                                                                   \
      b = b ^ (a << 16);                                                                           \
      c = c - a;                                                                                   \
      c = c - b;                                                                                   \
      c = c ^ (b >> 5);                                                                            \
      a = a - b;                                                                                   \
      a = a - c;                                                                                   \
      a = a ^ (c >> 3);                                                                            \
      b = b - c;                                                                                   \
      b = b - a;                                                                                   \
      b = b ^ (a << 10);                                                                           \
      c = c - a;                                                                                   \
      c = c - b;                                                                                   \
      c = c ^ (b >> 15);                                                                           \
    }

  uint32_t generate_id(const uint8_t *k, uint32_t bufferLength) {
    uint32_t a, b, c;
    uint32_t len;

    len = bufferLength;
    a = b = 0x9e3779b9;
    c     = 0;

    // handle most of the key
    while (len >= 12) {
      a = a + *((uint32_t *)&k[0]); //(k[0] + ((uint32_t)k[1] << 8) + ((uint32_t)k[ 2] << 16) +
                                    //((uint32_t)k[ 3] << 24));
      b = b + *((uint32_t *)&k[4]); //(k[4] + ((uint32_t)k[5] << 8) + ((uint32_t)k[ 6] << 16) +
                                    //((uint32_t)k[ 7] << 24));
      c = c + *((uint32_t *)&k[8]); //(k[8] + ((uint32_t)k[9] << 8) + ((uint32_t)k[10] << 16) +
                                    //((uint32_t)k[11] << 24));
      mix(a, b, c);
      k = k + 12;
      len -= 12;
    }

    /*------------------------------------- handle the last 11 bytes */
    c = c + bufferLength;
    switch (len) {
    case 11:
      c = c + ((uint32_t)k[10] << 24);
      [[fallthrough]];
    case 10:
      c = c + ((uint32_t)k[9] << 16);
      [[fallthrough]];
    case 9:
      c = c + ((uint32_t)k[8] << 8);
      [[fallthrough]];
      /* the first byte of c is reserved for the length */
    case 8:
      b = b + ((uint32_t)k[7] << 24);
      [[fallthrough]];
    case 7:
      b = b + ((uint32_t)k[6] << 16);
      [[fallthrough]];
    case 6:
      b = b + ((uint32_t)k[5] << 8);
      [[fallthrough]];
    case 5:
      b = b + k[4];
      [[fallthrough]];
    case 4:
      a = a + ((uint32_t)k[3] << 24);
      [[fallthrough]];
    case 3:
      a = a + ((uint32_t)k[2] << 16);
      [[fallthrough]];
    case 2:
      a = a + ((uint32_t)k[1] << 8);
      [[fallthrough]];
    case 1:
      a = a + k[0];
      /* case 0: nothing left to add */
    }
    mix(a, b, c);

    return c;
  }

#endif

bool BooksDir::read_books_directory(char *book_filename, int16_t &book_index) {
  LOG_D("Reading books directory: %s.", BOOKS_DIR_FILE);

  if (!db->open(BOOKS_DIR_FILE)) {
    LOG_E("Can't open database: %s", BOOKS_DIR_FILE);
    return false;
  }

  #if DEBUGGING
    show_db();
  #endif

  // We first verify if the database content is of the current version

  bool version_ok = false;
  VersionRecord version_record;

  if (db->get_record_count() == 0) {
    memset(&version_record, 0, sizeof(version_record));

    version_record.version = BOOKS_DIR_DB_VERSION;
    strcpy(version_record.app_name, APP_NAME);

    if (!db->add_record(&version_record, sizeof(version_record))) {
      LOG_E("Not able to set DB Version.");
      return false;
    }
    version_ok = true;
  } else {
    db->goto_first();
    if (db->get_record_size() == sizeof(version_record)) {
      db->get_record(&version_record, sizeof(version_record));
      if ((version_record.version == BOOKS_DIR_DB_VERSION) &&
          (strcmp(version_record.app_name, APP_NAME) == 0)) {
        version_ok = true;
      }
    }
  }

  if (!version_ok) {

    LOG_I("Database is of a wrong version or doesn't exists. Initializing...");

    if (!db->create(BOOKS_DIR_FILE)) {
      LOG_E("Unable to create database: %s", BOOKS_DIR_FILE);
      return false;
    }

    memset(&version_record, 0, sizeof(version_record));
    version_record.version = BOOKS_DIR_DB_VERSION;
    strcpy(version_record.app_name, APP_NAME);

    if (!db->add_record(&version_record, sizeof(version_record))) {
      LOG_E("Not able to set DB Version.");
      return false;
    }
  }

  if (!refresh(book_filename, book_index)) {
    LOG_E("Unable to complete DB refresh");
    return false;
  }

  // show_db();

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

auto BooksDir::get_book_data(uint16_t idx) -> EBookRecordPtr {
  if (idx >= sorted_index.size()) {
    LOG_E("Idx too large: %d", idx);
    return nullptr;
  }

  int i         = 0;
  int16_t index = -1;

  for (auto &entry : sorted_index) {
    if (idx == i) {
      index = entry.second.db_index;
      break;
    }
    i++;
  }
  if (index == -1) {
    LOG_E("Unable to find idx: %d", idx);
    return nullptr;
  }

  db->set_current_idx(index);

  size_t record_size = db->get_record_size();

  BooksDir::EBookRecordPtr book{nullptr};

  if (record_size >= sizeof(EBookRecord)) {
    book = EBookRecord::Make(record_size);
    if (book && !db->get_record(book.get(), record_size)) {
      LOG_E("Unable to get record at index %d", index);
      book.reset();
    }
  } else {
    LOG_E("Record size too small: %d", record_size);
  }

  return book;
}

bool BooksDir::get_book_id(uint16_t idx, uint32_t &id) {
  if (idx >= sorted_index.size()) {
    LOG_E("Idx too large: %d", idx);
    return false;
  }

  int i      = 0;
  bool found = false;

  for (auto &entry : sorted_index) {
    if (idx == i) {
      id    = entry.second.id;
      found = true;
      break;
    }
    i++;
  }
  if (!found) LOG_E("Unable to find idx: %d", idx);

  return found;
}

bool BooksDir::get_book_index(uint32_t id, uint16_t &idx) {
  int i      = 0;
  bool found = false;

  for (auto &entry : sorted_index) {
    if (entry.second.id == id) {
      idx   = i;
      found = true;
      break;
    }
    i++;
  }
  if (!found) LOG_E("Unable to find id: 0x%08" PRIx32, id);

  return found;
}

void BooksDir::set_track_order(uint32_t id, int8_t pos) {
  static bool no_recurse = false;
  if (no_recurse) return;

  LOG_D("-------------------------> set_track_order(%" PRIu32 ", %" PRIi8 ")", id, pos);
  bool found = false;

  for (auto &entry : sorted_index) {
    if (entry.second.id == id) {
      char ch = (pos >= 0) ? 'a' + pos : 'z';
      LOG_D("Old key: %s", entry.first.c_str());
      if (entry.first.front() != ch) {
        auto e          = sorted_index.extract(entry.first);
        e.key().front() = ch;
        LOG_D("New key: %s", e.key().c_str());
        sorted_index.insert(std::move(e));
      }
      found = true;
      break;
    }
  }

  if (!found) {
    #if EPUB_INKPLATE_BUILD
      no_recurse = true;
      nvs_mgr.erase(id);
      no_recurse = false;

    #endif
  }
}

#if 0
auto BooksDir::get_book_data_from_db_index(uint16_t idx) -> EBookRecordPtr {
  db->set_current_idx(idx);

  EBookRecordPtr book{nullptr};

  size_t record_size = db->get_record_size();
  if (record_size >= sizeof(EBookRecord)) {
    book = EBookRecord::Make(record_size);
    if (book && !db->get_record(book.get(), record_size)) {
      LOG_E("Unable to get record for db index %d", idx);
      book.reset();
    }
  } else {
    LOG_E("Record size too small: %d", record_size);
  }

  return book;
}
#endif

void BooksDir::clear_db() {
  db->goto_first();
  while (db->goto_next()) {
    db->set_deleted();
  }
}

void BooksDir::set_cover_size() {
  int8_t cover_size = 0;
  config.get(Config::Ident::COVER_SIZE, &cover_size);
  switch (cover_size) {
  case 0:
    cover_dim.width  = SMALL_COVER_WIDTH;
    cover_dim.height = SMALL_COVER_HEIGHT;
    break;
  case 1:
    cover_dim.width  = MEDIUM_COVER_WIDTH;
    cover_dim.height = MEDIUM_COVER_HEIGHT;
    break;
  case 2:
    cover_dim.width  = LARGE_COVER_WIDTH;
    cover_dim.height = LARGE_COVER_HEIGHT;
    break;
  default:
    cover_dim.width  = MEDIUM_COVER_WIDTH;
    cover_dim.height = MEDIUM_COVER_HEIGHT;
  }
}

/**
 * @brief Validates and synchronizes the database with the file system, removing stale entries
 *        and building an index of available books.
 *
 * This method iterates through all book records in the database, checks if the corresponding
 * files exist in the file system with matching sizes, and removes outdated entries. For valid
 * books, it builds a sorted index for quick lookup and retrieval.
 *
 * @param[in] book_filename Optional filename to search for and retrieve its database index.
 *                          If nullptr, no specific book search is performed.
 * @param[out] book_index   The database index of the book matching book_filename.
 *                          Only set if book_filename is provided and a match is found.
 * @param[out] temp_index   A temporary index mapping filenames to their IndexInfo (id and
 * db_index). Used for intermediate storage during the validation process.
 *
 * @details
 * - Allocates a temporary PartialRecord structure to read database entries
 * - Removes database records if the corresponding file doesn't exist or has mismatched file size
 * - For valid books, constructs a sorted index with a special prefix character:
 *   - On EPUB_INKPLATE_BUILD: prefix is based on position from NVS manager ('a'-'z')
 *   - Otherwise: prefix is 'z'
 * - Logs book availability and title information
 *
 * @note If memory allocation fails, calls msg_viewer.out_of_memory() and does not return.
 *
 * @see nvs_mgr, db, sorted_index, BOOKS_FOLDER, FILENAME_SIZE, TITLE_SIZE
 */
void BooksDir::check_db_content(char *book_filename, int16_t &book_index, SortedIndex &temp_index) {

  auto partial_record = PartialRecord::Make();

  if (!partial_record) {
    MsgViewer::out_of_memory("partial record allocation");
    // Will not return...
  }

  db->goto_first();

  while (db->goto_next()) { // Go pass the DB version record
    db->get_record(partial_record.get(), sizeof(PartialRecord));

    std::string fname = BOOKS_FOLDER "/";
    fname.append(partial_record->filename);

    struct stat stat_buffer;

    // if file with filename not found or the file size is not the same,
    // remove the database entry
    if ((stat(fname.c_str(), &stat_buffer) != 0) ||
        (stat_buffer.st_size != partial_record->file_size)) {
      LOG_D("Book no longer available: %s", partial_record->filename);
      db->set_deleted();
    } else {
      LOG_D("Title: %s", partial_record->title);
      temp_index[partial_record->filename] = IndexInfo{.id = 0, .db_index = 0};

      #if EPUB_INKPLATE_BUILD
        int8_t pos        = nvs_mgr.get_pos(partial_record->id);
        std::string title = " ";
        title += partial_record->title;
        title.front() = (pos >= 0) ? 'a' + pos : 'z';
      #else
        std::string title = "z";
        title += partial_record->title;
      #endif

      sorted_index[title] = IndexInfo{.id = partial_record->id, .db_index = db->get_current_idx()};
      if (book_filename) {
        if (strcmp(book_filename, partial_record->filename) == 0)
          book_index = db->get_current_idx();
      }
    }
  }
}

/**
 * @brief Cleans up and reorganizes the books database, rebuilding it with sorted index.
 *
 * This method defragments the internal database by creating a new database file,
 * copying all records from the old database, and rebuilding the sorted index.
 * The sorted index uses book titles with position prefixes for ordering.
 *
 * @param book_filename Optional filename to search for in the database. If provided and found,
 *                       its corresponding database index will be stored in book_index.
 * @param book_index Output parameter that receives the database index of the book matching
 *                   book_filename, if found. Only modified if book_filename is provided and
 *                   a matching record is found.
 *
 * @return true if the cleanup operation completed successfully, false otherwise.
 *         Returns false if any of the following fail:
 *         - Creating the new database
 *         - Reading from the old database
 *         - Allocating memory for records
 *         - Writing records to the new database
 *         - File system operations (remove/rename)
 *         - Opening the reorganized database
 *
 * @note The first record in the database is treated as a version record and is
 *       not included in the sorted index.
 * @note On EPUB_INKPLATE_BUILD, book titles are prefixed with a position indicator
 *       ('a'-'z' from NVS manager, or 'z' if position not found).
 *       On other builds, titles are simply prefixed with 'z'.
 * @note This operation closes and recreates the database file on disk.
 */
auto BooksDir::cleanup_db(char *book_filename, int16_t &book_index) -> bool {
  SimpleDBPtr new_db = SimpleDB::Make();
  sorted_index.clear();

  if (new_db->create(NEW_DIR_FILE)) {
    if (!db->goto_first()) {
      LOG_E("db->goto_first() failed");
      return false;
    }
    bool first = true; // First record is the version record, we want to keep it as is and not put
                       // it in the sorted index
    do {
      size_t size = db->get_record_size();
      if (first) {

        if (size < sizeof(VersionRecord)) {
          LOG_E("Unable to get proper record size: %zu from db", size);
          return false;
        }
        VersionRecordPtr data = VersionRecord::Make();
        if (!data) {
          LOG_E("Unable to allocate %zu bytes for version record", size);
          return false;
        }
        if (!db->get_record(data.get(), size)) {
          LOG_E("Unable to get version record of size %zu from db", size);
          return false;
        }
        if (!new_db->add_record(data.get(), size)) {
          LOG_E("Unable to add version record to db");
          return false;
        }

        first = false;
      } else {
        if (size < sizeof(EBookRecord)) {
          LOG_E("Unable to get proper record size: %zu from db", size);
          return false;
        }
        EBookRecordPtr data = EBookRecord::Make(size);
        if (!data) {
          LOG_E("Unable to allocate %zu bytes for ebook record", size);
          return false;
        }
        if (!db->get_record(data.get(), size)) {
          LOG_E("Unable to get record of size %zu from db", size);
          return false;
        }
        if (!new_db->add_record(data.get(), size)) {
          LOG_E("Unable to add record to db");
          return false;
        }

        uint16_t idx = new_db->get_record_count() - 1;
        #if EPUB_INKPLATE_BUILD
          int8_t pos        = nvs_mgr.get_pos(data->id);
          std::string title = " ";
          title += data->title;
          title.front() = (pos >= 0) ? 'a' + pos : 'z';
        #else
          std::string title = "z";
          title += data->title;
        #endif
        sorted_index[title] = IndexInfo{.id = data->id, .db_index = idx};
        if (book_filename) {
          if (strcmp(book_filename, data->filename) == 0)
            book_index = new_db->get_record_count() - 1;
        }
      }
    } while (db->goto_next());

    db->close();
    new_db->close();

    if (remove(BOOKS_DIR_FILE)) {
      LOG_E("Unable to remove directory DB file.");
      return false;
    }
    if (rename(NEW_DIR_FILE, BOOKS_DIR_FILE)) {
      LOG_E("Unable to rename new directory DB file");
      return false;
    }
    if (!db->open(BOOKS_DIR_FILE)) {
      LOG_E("Inable to open directory DB File.");
      return false;
    }
  }

  return true;
}

/**
 * @brief Loads new e-book files from the books folder into the database.
 *
 * Scans the books folder for new EPUB files that are not yet in the database.
 * For each new book found, extracts metadata (title, author, description),
 * retrieves and resizes the cover image, and adds a new record to the database.
 * Updates the sorted index with the newly added books.
 *
 * @param book_filename Optional filename to search for and return its database index.
 *                       If provided and found, book_index will be set to its position.
 *                       Can be nullptr if not needed.
 * @param book_index    Output parameter. Set to the database index if book_filename
 *                      matches a newly added book. Only modified if book_filename is provided
 *                      and a matching book is added.
 * @param temp_index    Reference to a sorted index (map) containing existing books in the database.
 *                      Used to identify which EPUB files are new.
 *
 * @return std::pair<bool, bool>
 *         - first:  Operation result status (true if successful, false on error)
 *         - second: Whether any new books were added (true if at least one book was added,
 *                   false if no new books were found or if the operation failed)
 *
 * @note Displays user messages through msg_viewer during the metadata retrieval process.
 *       Closes and reopens the database file after adding records to ensure data is
 *       properly written to storage.
 *       Resizes book covers to match cover_dim dimensions while maintaining aspect ratio.
 */
auto BooksDir::load_new_books_to_db(char *book_filename, int16_t &book_index,
                                    BooksDir::SortedIndex &temp_index) -> std::pair<bool, bool> {

  struct dirent *de = nullptr;
  DIR *dp           = nullptr;

  LOG_D("Looking at book files in folder %s", BOOKS_FOLDER);

  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif

  bool some_added_record = false;
  bool result            = true;

  int file_count = 0;
  dp             = opendir(BOOKS_FOLDER);
  while ((de = readdir(dp))) {
    int16_t size = strlen(de->d_name);
    if ((size > 5) && (strcasecmp(&de->d_name[size - 5], ".epub") == 0) &&
        (temp_index.find(de->d_name) == temp_index.end())) {
      file_count++;
    }
  }
  closedir(dp);

  LOG_D("Found %d new book files in the folder.", file_count);

  if (file_count == 0) {
    return {true, false};
  }

  dp = opendir(BOOKS_FOLDER);

  if (dp != nullptr) {

    bool first = true;

    while ((de = readdir(dp))) {

      int16_t size = strlen(de->d_name);
      if ((size > 5) && (strcasecmp(&de->d_name[size - 5], ".epub") == 0)) {

        std::string fname = de->d_name;

        // check if ebook file named fname is in the database

        if (temp_index.find(fname) == temp_index.end()) {

          // The book is not in the database, we add it now

          if (first) {
            first = false;
            // MsgViewer::show_progress("Computing new books pages location...");
            if (db->get_record_count() == 1) {
              MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "E-books metadata retrieval",
                              "System parameters changed requiring metadata retrieval. "
                              "It will take between 5 and 10 seconds for each book.");
            } else {
              MsgViewer::show(
                  MsgViewer::MsgType::INFO, false, true, "New e-books metadata retrieval",
                  "New e-books have been found (%s). Please wait while we retrieve some metadata. "
                  "It will take between 5 and 10 seconds for each e-book.",
                  de->d_name);
            }
          }
          some_added_record = true;

          LOG_D("New book found: %s", de->d_name);

          fname = BOOKS_FOLDER "/";
          fname.append(de->d_name);

          int32_t file_size = 0;
          struct stat stat_buffer;
          if (stat(fname.c_str(), &stat_buffer) != 0) {
            LOG_E("Unable to get stats for file: %s", fname.c_str());
            result = false;
            break;
          } else {
            file_size = stat_buffer.st_size;
          }

          LOG_D("Opening file through the EPub class: %s", fname.c_str());

          auto epub = EPub::Make();

          if (epub->open(fname)) {
            std::string filename = epub->get_cover_filename();

            PicturePtr pict;
            if (!filename.empty()) {

              // LOG_D("Cover filename: %s", filename);
              pict = epub->get_picture(filename, true);
              if (!pict) {
                LOG_D("Unable to retrieve cover file: %s", filename.c_str());
                pict = PictureFactory::create(default_cover_dim, default_cover,
                                              default_cover_dim.width * default_cover_dim.height);
              }
            } else {
              pict = PictureFactory::create(default_cover_dim, default_cover,
                                            default_cover_dim.width * default_cover_dim.height);
            }
            LOG_D("Picture: width: %d height: %d", pict->get_dim().width, pict->get_dim().height);

            int32_t w = cover_dim.width;
            int32_t h = pict->get_dim().height * cover_dim.width / pict->get_dim().width;

            if (h > cover_dim.height) {
              h = cover_dim.height;
              w = pict->get_dim().width * cover_dim.height / pict->get_dim().height;
            }

            pict->resize(Dim(w, h));

            auto book_record_size   = sizeof(EBookRecord) + w * h;
            EBookRecordPtr the_book = EBookRecord::Make(book_record_size);

            if (!the_book) {
              LOG_E("Not enough memory for new book: %d bytes required.", book_record_size);
              result = false;
              break;
            }

            memset((void *)the_book.get(), 0, sizeof(EBookRecord));
            memcpy(the_book->cover_bitmap, pict->get_bitmap(), w * h);

            the_book->cover_dim = Dim(w, h);

            LOG_D("Retrieving metadata");
            strlcpy(the_book->filename, de->d_name, FILENAME_SIZE);
            the_book->file_size = file_size;
            the_book->id = generate_id((uint8_t *)the_book->filename, strlen(the_book->filename));

            const char *str;

            if ((str = epub->get_title())) strlcpy(the_book->title, str, TITLE_SIZE);
            if ((str = epub->get_author())) strlcpy(the_book->author, str, AUTHOR_SIZE);
            if ((str = epub->get_description()))
              strlcpy(the_book->description, str, DESCRIPTION_SIZE);

            if (!db->add_record(the_book.get(), book_record_size)) {
              LOG_E("Unable to add a new record to DB file.");
              result = false;
              break;
            }

            uint16_t idx = db->get_record_count() - 1;
            #if EPUB_INKPLATE_BUILD
              int8_t pos        = nvs_mgr.get_pos(the_book->id);
              std::string title = " ";
              title += the_book->title;
              title.front() = (pos >= 0) ? 'a' + pos : 'z';
            #else
              std::string title = "z";
              title += the_book->title;
            #endif
            sorted_index[title] = {.id = the_book->id, .db_index = idx};

            if (book_filename) {
              if (strcmp(book_filename, the_book->filename) == 0)
                book_index = db->get_record_count() - 1;
            }

            epub->close_file();

            #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
              ESP::show_heaps_info();
            #endif
          }
        }
      }
    }

    closedir(dp);
  }

  if (some_added_record) {
    db->close(); // To ensure that data is well written on SD Card
    if (!db->open(BOOKS_DIR_FILE)) {
      LOG_E("Unable to open db file");
      result = false;
    }
  }

  return {result, some_added_record};
}

/**
 * @brief Refreshes the books directory database by scanning for new e-books and updating existing
 * records.
 *
 * This method performs a comprehensive refresh of the e-books database:
 * - Validates existing database entries against the file system
 * - Removes deleted or modified entries from the database
 * - Scans the books folder for new EPUB files
 * - Extracts metadata (title, author, description) and cover images from new e-books
 * - Maintains a sorted index of books with configurable cover sizes
 * - Supports selective initialization with force_init parameter
 *
 * @param book_filename Optional pointer to a specific book filename to locate its index.
 *                      If provided and found, book_index will be set to its database index.
 * @param book_index Reference to store the database index of the book matching book_filename.
 *                   Only modified if book_filename is provided and the book is found.
 * @param force_init If true, removes all existing database records before refresh, forcing complete
 *                   re-initialization. If false, performs incremental update.
 *
 * @return true if the refresh completed successfully; false if an error occurred during the
 * process. On failure, the database state may be inconsistent and should be re-opened.
 *
 * @note This operation is time-intensive as it:
 *       - Performs file system stat operations for existing entries
 *       - Opens and parses EPUB files to extract metadata and cover images
 *       - May take 5-10 seconds per new book on embedded systems
 *       - Displays progress messages to the user via msg_viewer
 *
 * @warning May temporarily allocate significant memory for cover images and records.
 *          Calls out_of_memory() via msg_viewer if allocation fails.
 */
bool BooksDir::refresh(char *book_filename, int16_t &book_index, bool force_init) {
  //  First look if existing entries in the database exists as ebook.
  //  Build a list of filenames for next step.

  LOG_D("Refreshing database content");

  SortedIndex temp_index;

  sorted_index.clear();

  set_cover_size();

  if (force_init) {

    clear_db();

  } else {

    check_db_content(book_filename, book_index, temp_index);
  }

  if (db->some_records_were_deleted()) {

    // Some record have been deleted. We have to recreate a database
    // with the cleaned records

    if (!cleanup_db(book_filename, book_index)) {
      temp_index.clear();
      return false;
    }
  }

  // Find ebooks that are new since last database refresh

  auto [result, some_added_record] = load_new_books_to_db(book_filename, book_index, temp_index);

  temp_index.clear();

  return result;
}

void BooksDir::show_db() {
  #if DEBUGGING
    VersionRecord version_record;
    EBookRecordPtr book;

    if (!db->goto_first()) return;

    if (!db->get_record(&version_record, sizeof(VersionRecord))) return;

    std::cout << "DB Version: " << version_record.version << " app: " << version_record.app_name
              << " record count: " << db->get_record_count() - 1 << std::endl;

    while (db->goto_next()) {
      size_t record_size = db->get_record_size();
      if (record_size >= sizeof(EBookRecord)) {
        book = EBookRecord::Make(record_size);
        if (!db->get_record(book.get(), record_size)) return;
        std::cout << "Book: " << book->filename << std::endl
                  << "  id: " << book->id << std::endl
                  << "  title: " << book->title << std::endl
                  << "  author: " << book->author << std::endl
                  << "  description: " << book->description << std::endl
                  << "  bitmap size: " << +book->cover_dim.width << " " << +book->cover_dim.height
                  << std::endl;
      } else {
        std::cout << "Record size too small: " << record_size << std::endl;
        continue;
      }
    }
  #endif
}