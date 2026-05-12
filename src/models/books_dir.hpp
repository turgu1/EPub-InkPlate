// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"
#include "simple_db.hpp"

#include "models/epub.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

/**
 * @brief Books Directory class
 *
 * This is where the list of available books is maintained. It offers
 * methods to read the directory from a database file located in the same folder
 * as the books themselves, refresh the list reading again all file content
 * not found in the database to retrieve meta-data.
 */
class BooksDir {
public:
  static constexpr uint16_t BOOKS_DIR_DB_VERSION = 7;

  static constexpr uint8_t FILENAME_SIZE     = 128;
  static constexpr uint8_t TITLE_SIZE        = 128;
  static constexpr uint8_t AUTHOR_SIZE       = 64;
  static constexpr uint16_t DESCRIPTION_SIZE = 512;

  static constexpr uint16_t SMALL_COVER_WIDTH  = 70;
  static constexpr uint16_t SMALL_COVER_HEIGHT = 90;

  static constexpr uint16_t MEDIUM_COVER_WIDTH  = 140;
  static constexpr uint16_t MEDIUM_COVER_HEIGHT = 180;

  static constexpr uint16_t LARGE_COVER_WIDTH  = 180;
  static constexpr uint16_t LARGE_COVER_HEIGHT = 240;

  /**
   * @brief Single EBook Record
   *
   * This represents the meta-data contained in the database for each epub book.
   * These are required to present the list of books to the user.
   * One element is not present in the structure: the list of pages location.
   * The *get_page_locs()* method is retrieving the pages location when required
   * by the epub class.
   */
  #pragma pack(push, 1)
  class EBookRecord;
  using EBookRecordPtr = HimemUniquePtr<EBookRecord>;

  class EBookRecord {
  private:
    EBookRecord() = default;

  public:
    ~EBookRecord() = default;

    template <typename T>
      requires(!std::is_array_v<T>)
    friend auto makeUniqueHimem(himemSizedT sz) -> HimemUniquePtr<T>;

    static inline auto Make(int32_t size) -> HimemUniquePtr<EBookRecord> {
      if (size >= static_cast<int32_t>(sizeof(EBookRecord))) {
        return makeUniqueHimem<EBookRecord>(himemSized(size));
      }
      return nullptr;
    }

    char filename[FILENAME_SIZE];       ///< Ebook filename, no folder
                                        ///  MUST STAY AS FIRST ITEM IN EBookRecord
    int32_t fileSize;                   ///< File size in bytes
    uint32_t id;                        ///< (Almost) Unique id computed from the filename
    char title[TITLE_SIZE];             ///< Title from epub meta-data
    char author[AUTHOR_SIZE];           ///< Author from epub meta-data
    char description[DESCRIPTION_SIZE]; ///< Description from epub meta-data
    Dim coverDim;                       ///< Dimensions of the cover bitmap
    uint8_t coverBitmap[];              ///< Cover bitmap shrinked for books list presentation

    auto coverSize() const -> uint32_t { return coverDim.width * coverDim.height; }
  };

  class PartialRecord {
  public:
    char filename[FILENAME_SIZE];
    int32_t fileSize;
    uint32_t id;
    char title[TITLE_SIZE];
    PartialRecord()  = default;
    ~PartialRecord() = default;
    static inline auto Make() { return makeUniqueHimem<PartialRecord>(); }
  };
  using PartialRecordPtr = HimemUniquePtr<PartialRecord>;

  // The version record is used to identify the version of the database. In case of structure
  // update, the version will be changed in the application and will trigger the reconstruction of
  // the database.
  //
  // MUST STAY AS FIRST ITEM IN THE DB FILE and THE STRUCT FORMAT MUST NEVER CHANGE!!
  class VersionRecord {
  public:
    uint16_t version;
    char appName[32];
    VersionRecord()  = default;
    ~VersionRecord() = default;
    static inline auto Make() { return makeUniqueHimem<VersionRecord>(); }
  };
  using VersionRecordPtr = HimemUniquePtr<VersionRecord>;

  #pragma pack(pop)

private:
  static constexpr char const *TAG            = "BooksDir";
  static constexpr char const *BOOKS_DIR_FILE = MAIN_FOLDER "/books_dir.db";
  static constexpr char const *NEW_DIR_FILE   = MAIN_FOLDER "/new_dir.db";
  static constexpr char const *APP_NAME       = "EPUB-INKPLATE";

  SimpleDBPtr db; ///< The SimpleDB database

  /// @struct IndexInfo
  /// @brief Stores index information for book entries in the database.
  ///
  /// This struct maintains a mapping between a book's unique identifier and its
  /// corresponding database index position, allowing efficient lookup and retrieval
  /// of book data from the database.
  ///
  /// @var IndexInfo::id
  ///   The unique identifier for the book entry.
  ///
  /// @var IndexInfo::dbIndex
  ///   The index position of this entry in the database.
  struct IndexInfo {
    uint32_t id;
    uint16_t dbIndex;
  };

  using SortedIndex = HimemMap<HimemString, IndexInfo>; ///< Sorted map of book names and indexes.
  SortedIndex sortedIndex; ///< Books index pointing at the db index of each book

  auto clearDb() -> void;
  auto setCoverSize() -> void;
  auto checkDbContent(char *bookFilename, int16_t &bookIndex, SortedIndex &tempIndex) -> void;
  auto cleanupDb(char *bookFilename, int16_t &bookIndex) -> bool;
  auto loadNewBooksToDb(char *bookFilename, int16_t &bookIndex, SortedIndex &tempIndex)
      -> std::pair<bool, bool>;

public:
  BooksDir() : db(SimpleDB::Make()) {}
  ~BooksDir() {
    sortedIndex.clear();
    closeDb();
  }

  /**
   * @brief Get the number of books present in the ebooks database
   *
   * @return int16_t The number of ebooks present in the database
   */
  [[nodiscard]] inline auto getBookCount() const -> int16_t { return sortedIndex.size(); }

  /**
   * @brief Get an ebook meta-data
   *
   * This method retrieve the meta-data related to an ebook index.
   *
   * @param idx The index is a sequential number in the sorted list of ebooks, ranging 0 ..
   * getBookCount()-1.
   *
   * @return const EBookRecordPtr Pointer to an EBookRecord structure, or nullptr if not able to
   * retrieve the data.
   */
  auto getBookData(uint16_t idx) -> EBookRecordPtr;
  // auto getBookDataFromDbIndex(uint16_t idx) -> EBookRecordPtr;
  auto getBookId(uint16_t idx, uint32_t &id) -> bool;
  auto getBookIndex(uint32_t id, uint16_t &idx) -> bool;
  auto setTrackOrder(uint32_t id, int8_t pos) -> void;

  auto getSortedIdx(uint16_t dbIdx) -> int16_t {
    int i = 0;
    for (auto &entry : sortedIndex) {
      if (entry.second.dbIndex == dbIdx) {
        return i;
      }
      ++i;
    }
    return -1;
  }

  auto getSortedIdxFromId(uint32_t id) -> int16_t {
    int i = 0;
    for (auto entry : sortedIndex) {
      if (entry.second.id == id) {
        return i;
      }
      ++i;
    }
    return -1;
  }

  static Dim coverDim;

  /**
   * @brief Read and refresh the ebooks list database
   *
   * This method is called once at application startup to refresh and load the primary index of all
   * books present in the database. If the database does not exists, it will be created. The refresh
   * process is used to update the database in case books have been added or removed to/from the
   * book folder. It has been optimized to limit the time required to refresh (books already seen in
   * the database are not scanned again).
   *
   * A version record is present in the database. In case of structure update, the version will be
   * changed in the application and will trigger the reconstruction of the database.
   *
   * Each book is identified using the file name and the file size.
   *
   * @param bookFilename Filename for wich the calling method needs the index for
   * @param bookIndex    The index corresponding to the book filename
   * @return true  The database has been updated and ready.
   * @return false Some error happened.
   */
  auto readBooksDirectory(char *bookFilename, int16_t &bookIndex) -> bool;

  /**
   * @brief Refresh the database
   *
   * This method is called by the *readBooksDirectory()* method to refresh the database. It can
   * also be called by the user through some option menu entry to request a database refresh.
   *
   * @param bookFilename Filename for wich the calling method needs the index for
   * @param bookIndex    The index corresponding to the book filename
   * @param forceInit    Remove all entries and reindex all books
   * @return true  The refresh process completed successfully.
   * @return false Some error happened.
   */
  auto refresh(char *bookFilename, int16_t &bookIndex, bool forceInit = false) -> bool;

  /**
   * @brief Close the SimpleDB database
   *
   */
  auto closeDb() -> void {
    if (db) db->close();
  }

  auto showDb() -> void;
};

#if __BOOKS_DIR__
  BooksDir booksDir;
#else
  extern BooksDir booksDir;
#endif
