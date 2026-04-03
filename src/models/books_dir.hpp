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
#include <map>
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

  static constexpr uint16_t LARGE_COVER_WIDTH  = 200;
  static constexpr uint16_t LARGE_COVER_HEIGHT = 260;

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
  using EBookRecordPtr = himem_unique_ptr<EBookRecord>;

  class EBookRecord {
  private:
    EBookRecord() = default;

  public:
    ~EBookRecord() = default;

    template <typename T>
      requires(!std::is_array_v<T>)
    friend himem_unique_ptr<T> make_unique_himem(himem_sized_t sz);

    static inline auto Make(int32_t size) -> himem_unique_ptr<EBookRecord> {
      if (size >= static_cast<int32_t>(sizeof(EBookRecord))) {
        return make_unique_himem<EBookRecord>(himem_sized(size));
      }
      return nullptr;
    }

    char filename[FILENAME_SIZE];       ///< Ebook filename, no folder
                                        ///  MUST STAY AS FIRST ITEM IN EBookRecord
    int32_t file_size;                  ///< File size in bytes
    uint32_t id;                        ///< (Almost) Unique id computed from the filename
    char title[TITLE_SIZE];             ///< Title from epub meta-data
    char author[AUTHOR_SIZE];           ///< Author from epub meta-data
    char description[DESCRIPTION_SIZE]; ///< Description from epub meta-data
    Dim cover_dim;                      ///< Dimensions of the cover bitmap
    uint8_t cover_bitmap[];             ///< Cover bitmap shrinked for books list presentation

    uint32_t cover_size() const { return cover_dim.width * cover_dim.height; }
  };

  class PartialRecord {
  public:
    char filename[FILENAME_SIZE];
    int32_t file_size;
    uint32_t id;
    char title[TITLE_SIZE];
    PartialRecord()  = default;
    ~PartialRecord() = default;
    static inline auto Make() { return make_unique_himem<PartialRecord>(); }
  };
  using PartialRecordPtr = himem_unique_ptr<PartialRecord>;

  // The version record is used to identify the version of the database. In case of structure
  // update, the version will be changed in the application and will trigger the reconstruction of
  // the database.
  //
  // MUST STAY AS FIRST ITEM IN THE DB FILE and THE STRUCT FORMAT MUST NEVER CHANGE!!
  class VersionRecord {
  public:
    uint16_t version;
    char app_name[32];
    VersionRecord()  = default;
    ~VersionRecord() = default;
    static inline auto Make() { return make_unique_himem<VersionRecord>(); }
  };
  using VersionRecordPtr = himem_unique_ptr<VersionRecord>;

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
  /// @var IndexInfo::db_index
  ///   The index position of this entry in the database.
  struct IndexInfo {
    uint32_t id;
    uint16_t db_index;
  };

  using SortedIndex = std::map<std::string, IndexInfo>; ///< Sorted map of book names and indexes.
  SortedIndex sorted_index; ///< Books index pointing at the db index of each book

  void clear_db();
  void set_cover_size();
  void check_db_content(char *book_filename, int16_t &book_index, SortedIndex &temp_index);
  auto cleanup_db(char *book_filename, int16_t &book_index) -> bool;
  auto load_new_books_to_db(char *book_filename, int16_t &book_index, SortedIndex &temp_index)
      -> std::pair<bool, bool>;

public:
  BooksDir() : db(SimpleDB::Make()) {}
  ~BooksDir() {
    sorted_index.clear();
    close_db();
  }

  /**
   * @brief Get the number of books present in the ebooks database
   *
   * @return int16_t The number of ebooks present in the database
   */
  inline int16_t get_book_count() const { return sorted_index.size(); }

  /**
   * @brief Get an ebook meta-data
   *
   * This method retrieve the meta-data related to an ebook index.
   *
   * @param idx The index is a sequential number in the sorted list of ebooks, ranging 0 ..
   * get_book_count()-1.
   *
   * @return const EBookRecordPtr Pointer to an EBookRecord structure, or nullptr if not able to
   * retrieve the data.
   */
  auto get_book_data(uint16_t idx) -> EBookRecordPtr;
  // auto get_book_data_from_db_index(uint16_t idx) -> EBookRecordPtr;
  bool get_book_id(uint16_t idx, uint32_t &id);
  bool get_book_index(uint32_t id, uint16_t &idx);
  void set_track_order(uint32_t id, int8_t pos);

  int16_t get_sorted_idx(uint16_t db_idx) {
    int i = 0;
    for (auto entry : sorted_index) {
      if (entry.second.db_index == db_idx) {
        return i;
      }
      i++;
    }
    return -1;
  }

  int16_t get_sorted_idx_from_id(uint32_t id) {
    int i = 0;
    for (auto entry : sorted_index) {
      if (entry.second.id == id) {
        return i;
      }
      i++;
    }
    return -1;
  }

  static Dim cover_dim;

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
   * @param book_filename Filename for wich the calling method needs the index for
   * @param book_index    The index corresponding to the book filename
   * @return true  The database has been updated and ready.
   * @return false Some error happened.
   */
  bool read_books_directory(char *book_filename, int16_t &book_index);

  /**
   * @brief Refresh the database
   *
   * This method is called by the *read_books_directory()* method to refresh the database. It can
   * also be called by the user through some option menu entry to request a database refresh.
   *
   * @param book_filename Filename for wich the calling method needs the index for
   * @param book_index    The index corresponding to the book filename
   * @param force_init    Remove all entries and reindex all books
   * @return true  The refresh process completed successfully.
   * @return false Some error happened.
   */
  bool refresh(char *book_filename, int16_t &book_index, bool force_init = false);

  /**
   * @brief Close the SimpleDB database
   *
   */
  void close_db() {
    if (db) db->close();
  }

  void show_db();
};

#if __BOOKS_DIR__
  BooksDir books_dir;
#else
  extern BooksDir books_dir;
#endif
