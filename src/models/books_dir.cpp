// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// #undef DEBUGGING
// #define DEBUGGING 1

#define __BOOKS_DIR__ 1
#include "models/books_dir.hpp"

#include "config.hpp"
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

Dim BooksDir::coverDim{BooksDir::SMALL_COVER_WIDTH, BooksDir::SMALL_COVER_HEIGHT};

#if 0
  const uint32_t CRC32_INITIAL    = 0xFFFFFFFFUL;
  const uint32_t CRC32_POLYNOMIAL = 0x1EDC6F41UL;

  static uint32_t 
  generateId(const uint8_t * buffer, uint32_t bufferLength)
  {
    uint32_t i;
    int8_t   j;
    uint32_t mask;
    uint32_t crc = CRC32_INITIAL;

    for (i = 0; i < bufferLength; ++i) {
      crc ^= ((uint8_t *)buffer)[i];
      for (j = 7; j >= 0; --j) {
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

  auto generateId(const uint8_t *k, uint32_t bufferLength) -> uint32_t {
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

auto BooksDir::readBooksDirectory(char *bookFilename, int16_t &bookIndex) -> bool {
  LOG_D("Reading books directory: %s.", BOOKS_DIR_FILE);

  if (!db->open(BOOKS_DIR_FILE)) {
    LOG_E("Can't open database: %s", BOOKS_DIR_FILE);
    return false;
  }

  #if DEBUGGING
    showDb();
  #endif

  // We first verify if the database content is of the current version

  bool versionOk = false;
  VersionRecord versionRecord;

  if (db->getRecordCount() == 0) {
    memset(&versionRecord, 0, sizeof(versionRecord));

    versionRecord.version = BOOKS_DIR_DB_VERSION;
    strcpy(versionRecord.appName, APP_NAME);

    if (!db->addRecord(&versionRecord, sizeof(versionRecord))) {
      LOG_E("Not able to set DB Version.");
      return false;
    }
    versionOk = true;
  } else {
    db->gotoFirst();
    if (db->getRecordSize() == sizeof(versionRecord)) {
      db->getRecord(&versionRecord, sizeof(versionRecord));
      if ((versionRecord.version == BOOKS_DIR_DB_VERSION) &&
          (strcmp(versionRecord.appName, APP_NAME) == 0)) {
        versionOk = true;
      }
    }
  }

  if (!versionOk) {

    LOG_I("Database is of a wrong version or doesn't exists. Initializing...");

    if (!db->create(BOOKS_DIR_FILE)) {
      LOG_E("Unable to create database: %s", BOOKS_DIR_FILE);
      return false;
    }

    memset(&versionRecord, 0, sizeof(versionRecord));
    versionRecord.version = BOOKS_DIR_DB_VERSION;
    strcpy(versionRecord.appName, APP_NAME);

    if (!db->addRecord(&versionRecord, sizeof(versionRecord))) {
      LOG_E("Not able to set DB Version.");
      return false;
    }
  }

  if (!refresh(bookFilename, bookIndex)) {
    LOG_E("Unable to complete DB refresh");
    return false;
  }

  // showDb();

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

auto BooksDir::getBookData(uint16_t idx) -> EBookRecordPtr {
  if (idx >= sortedIndex.size()) {
    LOG_E("Idx too large: %d", idx);
    return nullptr;
  }

  int i         = 0;
  int16_t index = -1;

  for (auto &entry : sortedIndex) {
    if (idx == i) {
      index = entry.second.dbIndex;
      break;
    }
    ++i;
  }
  if (index == -1) {
    LOG_E("Unable to find idx: %d", idx);
    return nullptr;
  }

  db->setCurrentIdx(index);

  size_t recordSize = db->getRecordSize();

  BooksDir::EBookRecordPtr book{nullptr};

  if (recordSize >= sizeof(EBookRecord)) {
    book = EBookRecord::Make(recordSize);
    if (book && !db->getRecord(book.get(), recordSize)) {
      LOG_E("Unable to get record at index %d", index);
      book.reset();
    }
  } else {
    LOG_E("Record size too small: %d", recordSize);
  }

  return book;
}

auto BooksDir::getBookId(uint16_t idx, uint32_t &id) -> bool {
  if (idx >= sortedIndex.size()) {
    LOG_E("Idx too large: %d", idx);
    return false;
  }

  int i      = 0;
  bool found = false;

  for (auto &entry : sortedIndex) {
    if (idx == i) {
      id    = entry.second.id;
      found = true;
      break;
    }
    ++i;
  }
  if (!found) LOG_E("Unable to find idx: %d", idx);

  return found;
}

auto BooksDir::getBookIndex(uint32_t id, uint16_t &idx) -> bool {
  int i      = 0;
  bool found = false;

  for (auto &entry : sortedIndex) {
    if (entry.second.id == id) {
      idx   = i;
      found = true;
      break;
    }
    ++i;
  }
  if (!found) LOG_E("Unable to find id: 0x%08" PRIx32, id);

  return found;
}

auto BooksDir::setTrackOrder(uint32_t id, int8_t pos) -> void {
  static bool noRecurse = false;
  if (noRecurse) return;

  LOG_D("-------------------------> setTrackOrder(%" PRIu32 ", %" PRIi8 ")", id, pos);
  bool found = false;

  for (auto &entry : sortedIndex) {
    if (entry.second.id == id) {
      char ch = (pos >= 0) ? 'a' + pos : 'z';
      LOG_D("Old key: %s", entry.first.c_str());
      if (entry.first.front() != ch) {
        auto e          = sortedIndex.extract(entry.first);
        e.key().front() = ch;
        LOG_D("New key: %s", e.key().c_str());
        sortedIndex.insert(std::move(e));
      }
      found = true;
      break;
    }
  }

  if (!found) {
    #if EPUB_INKPLATE_BUILD
      noRecurse = true;
      nvsMgr.erase(id);
      noRecurse = false;

    #endif
  }
}

#if 0
auto BooksDir::getBookDataFromDbIndex(uint16_t idx) -> EBookRecordPtr {
  db->setCurrentIdx(idx);

  EBookRecordPtr book{nullptr};

  size_t recordSize = db->getRecordSize();
  if (recordSize >= sizeof(EBookRecord)) {
    book = EBookRecord::Make(recordSize);
    if (book && !db->getRecord(book.get(), recordSize)) {
      LOG_E("Unable to get record for db index %d", idx);
      book.reset();
    }
  } else {
    LOG_E("Record size too small: %d", recordSize);
  }

  return book;
}
#endif

auto BooksDir::clearDb() -> void {
  db->gotoFirst();
  while (db->gotoNext()) {
    db->setDeleted();
  }
}

auto BooksDir::setCoverSize() -> void {
  int8_t coverSize = 0;
  config.get(Config::Ident::COVER_SIZE, &coverSize);
  switch (coverSize) {
  case 0:
    coverDim.width  = SMALL_COVER_WIDTH;
    coverDim.height = SMALL_COVER_HEIGHT;
    break;
  case 1:
    coverDim.width  = MEDIUM_COVER_WIDTH;
    coverDim.height = MEDIUM_COVER_HEIGHT;
    break;
  case 2:
    coverDim.width  = LARGE_COVER_WIDTH;
    coverDim.height = LARGE_COVER_HEIGHT;
    break;
  default:
    coverDim.width  = MEDIUM_COVER_WIDTH;
    coverDim.height = MEDIUM_COVER_HEIGHT;
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
 * @param[in] bookFilename Optional filename to search for and retrieve its database index.
 *                          If nullptr, no specific book search is performed.
 * @param[out] bookIndex   The database index of the book matching bookFilename.
 *                          Only set if bookFilename is provided and a match is found.
 * @param[out] tempIndex   A temporary index mapping filenames to their IndexInfo (id and
 * dbIndex). Used for intermediate storage during the validation process.
 *
 * @details
 * - Allocates a temporary PartialRecord structure to read database entries
 * - Removes database records if the corresponding file doesn't exist or has mismatched file size
 * - For valid books, constructs a sorted index with a special prefix character:
 *   - On EPUB_INKPLATE_BUILD: prefix is based on position from NVS manager ('a'-'z')
 *   - Otherwise: prefix is 'z'
 * - Logs book availability and title information
 *
 * @note If memory allocation fails, calls msg_viewer.outOfMemory() and does not return.
 *
 * @see nvsMgr, db, sortedIndex, BOOKS_FOLDER, FILENAME_SIZE, TITLE_SIZE
 */
auto BooksDir::checkDbContent(char *bookFilename, int16_t &bookIndex, SortedIndex &tempIndex)
    -> void {

  auto partialRecord = PartialRecord::Make();

  if (!partialRecord) {
    MsgViewer::outOfMemory("partial record allocation");
    // Will not return...
  }

  db->gotoFirst();

  while (db->gotoNext()) { // Go pass the DB version record
    db->getRecord(partialRecord.get(), sizeof(PartialRecord));

    std::string fname = BOOKS_FOLDER "/";
    fname.append(partialRecord->filename);

    struct stat statBuffer;

    // if file with filename not found or the file size is not the same,
    // remove the database entry
    if ((stat(fname.c_str(), &statBuffer) != 0) ||
        (statBuffer.st_size != partialRecord->fileSize)) {
      LOG_D("Book no longer available: %s", partialRecord->filename);
      db->setDeleted();
    } else {
      LOG_D("Title: %s", partialRecord->title);
      tempIndex[partialRecord->filename] = IndexInfo{.id = 0, .dbIndex = 0};

      #if EPUB_INKPLATE_BUILD
        int8_t pos        = nvsMgr.getPos(partialRecord->id);
        HimemString title = " ";
        title += partialRecord->title;
        title.front() = (pos >= 0) ? 'a' + pos : 'z';
      #else
        HimemString title = "z";
        title += partialRecord->title;
      #endif

      sortedIndex[title] = IndexInfo{.id = partialRecord->id, .dbIndex = db->getCurrentIdx()};
      if (bookFilename) {
        if (strcmp(bookFilename, partialRecord->filename) == 0) bookIndex = db->getCurrentIdx();
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
 * @param bookFilename Optional filename to search for in the database. If provided and found,
 *                       its corresponding database index will be stored in bookIndex.
 * @param bookIndex Output parameter that receives the database index of the book matching
 *                   bookFilename, if found. Only modified if bookFilename is provided and
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
auto BooksDir::cleanupDb(char *bookFilename, int16_t &bookIndex) -> bool {
  SimpleDBPtr newDb = SimpleDB::Make();
  sortedIndex.clear();

  if (newDb->create(NEW_DIR_FILE)) {
    if (!db->gotoFirst()) {
      LOG_E("db->gotoFirst() failed");
      return false;
    }
    bool first = true; // First record is the version record, we want to keep it as is and not put
                       // it in the sorted index
    do {
      size_t size = db->getRecordSize();
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
        if (!db->getRecord(data.get(), size)) {
          LOG_E("Unable to get version record of size %zu from db", size);
          return false;
        }
        if (!newDb->addRecord(data.get(), size)) {
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
        if (!db->getRecord(data.get(), size)) {
          LOG_E("Unable to get record of size %zu from db", size);
          return false;
        }
        if (!newDb->addRecord(data.get(), size)) {
          LOG_E("Unable to add record to db");
          return false;
        }

        uint16_t idx = newDb->getRecordCount() - 1;
        #if EPUB_INKPLATE_BUILD
          int8_t pos        = nvsMgr.getPos(data->id);
          HimemString title = " ";
          title += data->title;
          title.front() = (pos >= 0) ? 'a' + pos : 'z';
        #else
          HimemString title = "z";
          title += data->title;
        #endif
        sortedIndex[title] = IndexInfo{.id = data->id, .dbIndex = idx};
        if (bookFilename) {
          if (strcmp(bookFilename, data->filename) == 0) bookIndex = newDb->getRecordCount() - 1;
        }
      }
    } while (db->gotoNext());

    db->close();
    newDb->close();

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
 * @param bookFilename Optional filename to search for and return its database index.
 *                       If provided and found, bookIndex will be set to its position.
 *                       Can be nullptr if not needed.
 * @param bookIndex    Output parameter. Set to the database index if bookFilename
 *                      matches a newly added book. Only modified if bookFilename is provided
 *                      and a matching book is added.
 * @param tempIndex    Reference to a sorted index (map) containing existing books in the database.
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
 *       Resizes book covers to match coverDim dimensions while maintaining aspect ratio.
 */
auto BooksDir::loadNewBooksToDb(char *bookFilename, int16_t &bookIndex,
                                BooksDir::SortedIndex &tempIndex) -> std::pair<bool, bool> {

  struct dirent *de = nullptr;
  DIR *dp           = nullptr;

  LOG_D("Looking at book files in folder %s", BOOKS_FOLDER);

  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif

  bool someAddedRecord = false;
  bool result          = true;

  int fileCount = 0;
  dp            = opendir(BOOKS_FOLDER);
  while ((de = readdir(dp))) {
    int16_t size = strlen(de->d_name);
    if ((size > 5) && (strcasecmp(&de->d_name[size - 5], ".epub") == 0) &&
        (tempIndex.find(de->d_name) == tempIndex.end())) {
      fileCount++;
    }
  }
  closedir(dp);

  LOG_D("Found %d new book files in the folder.", fileCount);

  if (fileCount == 0) {
    return {true, false};
  }

  dp = opendir(BOOKS_FOLDER);

  if (dp != nullptr) {

    bool first = true;

    while ((de = readdir(dp))) {

      int16_t size = strlen(de->d_name);
      if ((size > 5) && (strcasecmp(&de->d_name[size - 5], ".epub") == 0)) {

        HimemString fname = de->d_name;

        // check if ebook file named fname is in the database

        if (tempIndex.find(fname) == tempIndex.end()) {

          // The book is not in the database, we add it now

          if (first) {
            first = false;
            // MsgViewer::showProgress("Computing new books pages location...");
            if (db->getRecordCount() == 1) {
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
          someAddedRecord = true;

          LOG_D("New book found: %s", de->d_name);

          fname = BOOKS_FOLDER "/";
          fname.append(de->d_name);

          int32_t fileSize = 0;
          struct stat statBuffer;
          if (stat(fname.c_str(), &statBuffer) != 0) {
            LOG_E("Unable to get stats for file: %s", fname.c_str());
            result = false;
            break;
          } else {
            fileSize = statBuffer.st_size;
          }

          LOG_D("Opening file through the EPub class: %s", fname.c_str());

          auto epub = EPub::Make();

          if (epub->open(fname)) {
            HimemString filename = epub->getCoverFilename();

            PicturePtr pict;
            if (!filename.empty()) {

              // LOG_D("Cover filename: %s", filename);
              pict = epub->getPicture(filename, true);
              if (!pict) {
                LOG_D("Unable to retrieve cover file: %s", filename.c_str());
                pict = PictureFactory::create(defaultCoverDim, defaultCover,
                                              defaultCoverDim.width * defaultCoverDim.height);
              }
            } else {
              pict = PictureFactory::create(defaultCoverDim, defaultCover,
                                            defaultCoverDim.width * defaultCoverDim.height);
            }
            LOG_D("Picture: width: %d height: %d", pict->getDim().width, pict->getDim().height);

            int32_t w = coverDim.width;
            int32_t h = pict->getDim().height * coverDim.width / pict->getDim().width;

            if (h > coverDim.height) {
              h = coverDim.height;
              w = pict->getDim().width * coverDim.height / pict->getDim().height;
            }

            pict->resize(Dim(w, h));

            auto bookRecordSize    = sizeof(EBookRecord) + w * h;
            EBookRecordPtr theBook = EBookRecord::Make(bookRecordSize);

            if (!theBook) {
              LOG_E("Not enough memory for new book: %d bytes required.", bookRecordSize);
              result = false;
              break;
            }

            memset((void *)theBook.get(), 0, sizeof(EBookRecord));
            memcpy(theBook->coverBitmap, pict->getBitmap(), w * h);

            theBook->coverDim = Dim(w, h);

            LOG_D("Retrieving metadata");
            strlcpy(theBook->filename, de->d_name, FILENAME_SIZE);
            theBook->fileSize = fileSize;
            theBook->id       = generateId((uint8_t *)theBook->filename, strlen(theBook->filename));

            const char *str;

            if ((str = epub->getTitle())) strlcpy(theBook->title, str, TITLE_SIZE);
            if ((str = epub->getAuthor())) strlcpy(theBook->author, str, AUTHOR_SIZE);
            if ((str = epub->getDescription()))
              strlcpy(theBook->description, str, DESCRIPTION_SIZE);

            if (!db->addRecord(theBook.get(), bookRecordSize)) {
              LOG_E("Unable to add a new record to DB file.");
              result = false;
              break;
            }

            uint16_t idx = db->getRecordCount() - 1;
            #if EPUB_INKPLATE_BUILD
              int8_t pos        = nvsMgr.getPos(theBook->id);
              HimemString title = " ";
              title += theBook->title;
              title.front() = (pos >= 0) ? 'a' + pos : 'z';
            #else
              HimemString title = "z";
              title += theBook->title;
            #endif
            sortedIndex[title] = {.id = theBook->id, .dbIndex = idx};

            if (bookFilename) {
              if (strcmp(bookFilename, theBook->filename) == 0)
                bookIndex = db->getRecordCount() - 1;
            }

            epub->closeFile();

            #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
              ESP::show_heaps_info();
            #endif
          }
        }
      }
    }

    closedir(dp);
  }

  if (someAddedRecord) {
    db->close(); // To ensure that data is well written on SD Card
    if (!db->open(BOOKS_DIR_FILE)) {
      LOG_E("Unable to open db file");
      result = false;
    }
  }

  return {result, someAddedRecord};
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
 * - Supports selective initialization with forceInit parameter
 *
 * @param bookFilename Optional pointer to a specific book filename to locate its index.
 *                      If provided and found, bookIndex will be set to its database index.
 * @param bookIndex Reference to store the database index of the book matching bookFilename.
 *                   Only modified if bookFilename is provided and the book is found.
 * @param forceInit If true, removes all existing database records before refresh, forcing complete
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
 *          Calls outOfMemory() via msg_viewer if allocation fails.
 */
auto BooksDir::refresh(char *bookFilename, int16_t &bookIndex, bool forceInit) -> bool {
  //  First look if existing entries in the database exists as ebook.
  //  Build a list of filenames for next step.

  LOG_D("Refreshing database content");

  SortedIndex tempIndex;

  sortedIndex.clear();

  setCoverSize();

  if (forceInit) {

    clearDb();

  } else {

    checkDbContent(bookFilename, bookIndex, tempIndex);
  }

  if (db->someRecordsWereDeleted()) {

    // Some record have been deleted. We have to recreate a database
    // with the cleaned records

    if (!cleanupDb(bookFilename, bookIndex)) {
      tempIndex.clear();
      return false;
    }
  }

  // Find ebooks that are new since last database refresh

  auto [result, someAddedRecord] = loadNewBooksToDb(bookFilename, bookIndex, tempIndex);

  tempIndex.clear();

  return result;
}

auto BooksDir::showDb() -> void {
  #if DEBUGGING
    VersionRecord versionRecord;
    EBookRecordPtr book;

    if (!db->gotoFirst()) return;

    if (!db->getRecord(&versionRecord, sizeof(VersionRecord))) return;

    std::cout << "DB Version: " << versionRecord.version << " app: " << versionRecord.appName
              << " record count: " << db->getRecordCount() - 1 << std::endl;

    while (db->gotoNext()) {
      size_t recordSize = db->getRecordSize();
      if (recordSize >= sizeof(EBookRecord)) {
        book = EBookRecord::Make(recordSize);
        if (!db->getRecord(book.get(), recordSize)) return;
        std::cout << "Book: " << book->filename << std::endl
                  << "  id: " << book->id << std::endl
                  << "  title: " << book->title << std::endl
                  << "  author: " << book->author << std::endl
                  << "  description: " << book->description << std::endl
                  << "  bitmap size: " << +book->coverDim.width << " " << +book->coverDim.height
                  << std::endl;
      } else {
        std::cout << "Record size too small: " << recordSize << std::endl;
        continue;
      }
    }
  #endif
}