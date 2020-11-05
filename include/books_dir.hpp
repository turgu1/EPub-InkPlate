#ifndef __BOOKS_DIR_HPP__
#define __BOOKS_DIR_HPP__

#include "global.hpp"

#include "epub.hpp"

#include <vector>
#include <iostream>
#include <fstream>

#include "sqlite3.h"

#define BOOKS_DIR_DB_VERSION 1

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
    /**
     * @brief Single EBook Record
     * 
     * This represents the meta-data contained in the database for each epub book.
     * These are required to present the list of books to the user.
     * One element is not present in the structure: the list of page locations.
     * The *get_page_locs()* method is retrieving the page locations when required
     * by the epub class.
     */
    struct EBookRecord {
      std::string filename;          ///< Ebook filename, no folder
      int32_t file_size;             ///< File size in bytes
      std::string title;             ///< Title from epub meta-data
      std::string author;            ///< Author from epub meta-data
      std::string description;       ///< Description from epub meta-data
      unsigned char * cover_bitmap;  ///< Cover bitmap shrinked for books list presentation
      int16_t cover_width;           ///< Width of the cover bitmap
      int16_t cover_height;          ///< Height of the cover bitmap
    };

  private:
    sqlite3 * db; ///< The SQLite3 database structure

    typedef std::vector<int64_t> Ids;  ///< Vector of primary keys of books.
    Ids book_ids;                      ///< Books index pointing at the primary key of each book
    EBookRecord book;                  ///< Book Record structure prepared to return to the caller
    int16_t current_book_idx;          ///< Current book index present in the book structure

  public:
    BooksDir() : db(nullptr), current_book_idx(-1) {
      book.cover_bitmap = nullptr;
    }
   ~BooksDir() {
      book_ids.clear();
      delete [] book.cover_bitmap;
      close_db(); 
    }

    /**
     * @brief Get the number of books present in the book database
     * 
     * @return int16_t The number of books present in the database
     */
    inline int16_t get_book_count() const { return book_ids.size(); }

    /**
     * @brief Get a book meta-data
     * 
     * This method retrieve the meta-data related to a book index. 
     * 
     * @param idx The index is a sequential number in the list of books, ranging 0 .. get_book_count()-1.
     * 
     * @return const EBookRecord* Pointer to an EBookRecord structure, or NULL if not able to retrieve the data.
     */
    const EBookRecord * get_book_data(int16_t idx);

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
     * @brief Close the SQLite3 database
     * 
     */
    void close_db() { if (db) { sqlite3_close(db); db = nullptr; } }
};

#if __BOOKS_DIR__
  BooksDir books_dir;
#else
  extern BooksDir books_dir;
#endif

#endif