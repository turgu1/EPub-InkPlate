// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __BOOKS_DIR_HPP__
#define __BOOKS_DIR_HPP__

#include "global.hpp"

#include "epub.hpp"

#include <vector>
#include <map>
#include <iostream>
#include <fstream>

#include "simple_db.hpp"

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
    static const uint16_t BOOKS_DIR_DB_VERSION =   1;

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
     * One element is not present in the structure: the list of page locations.
     * The *get_page_locs()* method is retrieving the page locations when required
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
      uint8_t  pages_data[0];                 ///< Blob of pages data
    };

    struct VersionRecord {
      uint16_t version;
      char     app_name[32];
    };
    #pragma pack(pop)

  private:
    static constexpr char const * TAG = "BooksDir";

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
     * @param idx The index is a sequential number in the list of books, ranging 0 .. get_book_count()-1.
     * 
     * @return const EBookRecord* Pointer to an EBookRecord structure, or NULL if not able to retrieve the data.
     */
    const EBookRecord * get_book_data(uint16_t idx);

    /**
     * @brief Get page locations
     * 
     * This method retrieve a PageLocs vector from the database, related to a book index.
     * 
     * @param page_locs The vector that will be receiving the list of pages location.
     * @param idx  The index is a sequential number in the list of books, ranging 0 .. get_book_count()-1.
     * @return true  Retrieval is successful.
     * @return false Some error happened in retrieval.
     */
    bool get_page_locs(EPub::PageLocs & page_locs, int16_t idx);

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
     * The process can be long as each new book is scanned to compute the location of pages. This is required
     * to get a responsive interaction with the users when reading books.
     * 
     * Each book is identified using the file name et the file size.
     *  
     * @return true  The database has been updated and ready.
     * @return false Some error happened.
     */
    bool read_books_directory();

    /**
     * @brief Refresh the database
     * 
     * This method is called by the *read_books_directory()* method to refresh the database. It can also
     * be called by the user through some option menu entry to order a database refresh.
     * 
     * @return true  The refresh process completed successfully.
     * @return false Some error happened.
     */
    bool refresh();

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

#endif