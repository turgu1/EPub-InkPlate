// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"

#include "models/epub.hpp"

#include <vector>
#include <map>
#include <iostream>
#include <fstream>

#include "helpers/simple_db.hpp"

/**
 * @brief Books Directory class
 * 
 * This is where the list of available books is maintained. It offers
 * methods to read the directory from a database file located in the same folder 
 * as the books themselves, refresh the list reading again all file content
 * not found in the database to retrieve meta-data.
 */
class BooksDir
{
  public:
    static const uint16_t BOOKS_DIR_DB_VERSION =   5;

    static const uint8_t  FILENAME_SIZE        = 128;
    static const uint8_t  TITLE_SIZE           = 128;
    static const uint8_t  AUTHOR_SIZE          =  64;
    static const uint16_t DESCRIPTION_SIZE     = 512;

    static const uint8_t  MAX_COVER_WIDTH      =  70;
    static const uint8_t  MAX_COVER_HEIGHT     =  90;

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
    struct EBookRecord {
      char     filename[FILENAME_SIZE];       ///< Ebook filename, no folder
                                              ///  MUST STAY AS FIRST ITEM IN EBookRecord
      int32_t  file_size;                     ///< File size in bytes
      char     title[TITLE_SIZE];             ///< Title from epub meta-data
      char     author[AUTHOR_SIZE];           ///< Author from epub meta-data
      char     description[DESCRIPTION_SIZE]; ///< Description from epub meta-data
      uint8_t  cover_bitmap[MAX_COVER_WIDTH][MAX_COVER_HEIGHT];  ///< Cover bitmap shrinked for books list presentation
      uint8_t  cover_width;                   ///< Width of the cover bitmap
      uint8_t  cover_height;                  ///< Height of the cover bitmap
    };

    struct VersionRecord {
      uint16_t version;
      char     app_name[32];
    };
    #pragma pack(pop)

  private:
    static constexpr char const * TAG            = "BooksDir";
    static constexpr char const * BOOKS_DIR_FILE = MAIN_FOLDER "/books_dir.db";
    static constexpr char const * NEW_DIR_FILE   = MAIN_FOLDER "/new_dir.db";
    static constexpr char const * APP_NAME       = "EPUB-INKPLATE";

    SimpleDB db;                       ///< The SimpleDB database

    typedef std::map<std::string, int16_t> SortedIndex;  ///< Sorted map of book names and indexes.
    SortedIndex sorted_index;          ///< Books index pointing at the db index of each book
    EBookRecord book;                  ///< Book Record structure prepared to return to the caller
    int16_t current_book_idx;          ///< Current book index present in the book structure

  public:
    BooksDir() : current_book_idx(-1) { }
   ~BooksDir() {
      sorted_index.clear();
      close_db(); 
    }

    /**
     * @brief Get the number of books present in the book database
     * 
     * @return int16_t The number of books present in the database
     */
    inline int16_t get_book_count() const { return sorted_index.size(); }

    /**
     * @brief Get a book meta-data
     * 
     * This method retrieve the meta-data related to a book index. 
     * 
     * @param idx The index is a sequential number in the sorted list of books, ranging 0 .. get_book_count()-1.
     * 
     * @return const EBookRecord* Pointer to an EBookRecord structure, or NULL if not able to retrieve the data.
     */
    const EBookRecord * get_book_data(uint16_t idx);
    const EBookRecord * get_book_data_from_db_index(uint16_t idx);

    int16_t get_sorted_idx(int16_t db_idx) {
      int i = 0;
      for (auto entry : sorted_index) {
        if (entry.second == db_idx) {
          return i;
        }
        i++;
      }
      return -1;
    }

    static const int16_t max_cover_width  = MAX_COVER_WIDTH;  ///< Bitmap width in pixels to present a book cover in the list
    static const int16_t max_cover_height = MAX_COVER_HEIGHT; ///< Bitmap height in pixels

    /**
     * @brief Read and refresh the books list database
     * 
     * This method is called once at application startup to refresh and load the primary index of all
     * books present in the database. If the database does not exists, it will be created. The refresh
     * process is used to update the database in case books have been added or removed to/from the book
     * folder. It has been optimized to limit the time required to refresh (books already seen in the 
     * database are not scanned again).
     * 
     * A version record is present in the database. In case of structure update, the version will be changed in
     * the application and will trigger the reconstruction of the database.
     * 
     * Each book is identified using the file name and the file size.
     *  
     * @param book_filename Filename for wich the calling method needs the index for
     * @param book_index    The index corresponding to the book filename
     * @return true  The database has been updated and ready.
     * @return false Some error happened.
     */
    bool read_books_directory(char * book_filename, int16_t & book_index);

    /**
     * @brief Refresh the database
     * 
     * This method is called by the *read_books_directory()* method to refresh the database. It can also
     * be called by the user through some option menu entry to request a database refresh.
     * 
     * @param book_filename Filename for wich the calling method needs the index for
     * @param book_index    The index corresponding to the book filename
     * @param force_init    Remove all entries and reindex all books
     * @return true  The refresh process completed successfully.
     * @return false Some error happened.
     */
    bool refresh(char * book_filename, int16_t & book_index, bool force_init = false);

    /**
     * @brief Close the SimpleDB database
     * 
     */
    void close_db() { db.close(); }

    void show_db();
};

#if __BOOKS_DIR__
  BooksDir books_dir;
#else
  extern BooksDir books_dir;
#endif
