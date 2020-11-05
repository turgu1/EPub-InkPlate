#define __BOOKS_DIR__ 1
#include "books_dir.hpp"

#include "epub.hpp"
#include "book_view.hpp"
#include "logging.hpp"

#include "stb_image.h"
#include "stb_image_resize.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sstream>

static const char * TAG = "BooksDir";

static const char * BOOKS_DIR_FILE  = BOOKS_FOLDER "/books_dir.db";

bool 
BooksDir::read_books_directory()
{
  if (sqlite3_open(BOOKS_DIR_FILE, &db) != SQLITE_OK) {
    LOG_E(TAG, "Can't open database: %s", sqlite3_errmsg(db));
    return false;
  }

  char * err_msg;

  // We first verify if the database content is of the current version

  const char * sql = 
    "create table if not exists version("
    "id           integer primary key,"
    "version      int,"
    "app_name     text);";

  if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
    LOG_E(TAG, "SQL error (create table version): %s", err_msg);
    sqlite3_free(err_msg);
    return false;
  }

  sql = "select version, app_name from version limit 1;";
  sqlite3_stmt * stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_E(TAG, "SQL error (select version from version): %s", sqlite3_errmsg(db));
    return false;
  }

  int rc = sqlite3_step(stmt);
  int32_t version = -1;
  std::string app_name;

  if (rc == SQLITE_ROW) {
    version  = sqlite3_column_int (stmt, 0);
    app_name = (const char *) sqlite3_column_text(stmt, 1);
  }
  sqlite3_finalize(stmt);

  if ((version != BOOKS_DIR_DB_VERSION) || (app_name.compare(APP_NAME) != 0)) {

    LOG_I(TAG, "Database is of a wrong version or doesn't exists. Initializing...");

    sql = "drop table if exists books;";

    if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
      LOG_E(TAG, "SQL error (drop table books): %s", err_msg);
      sqlite3_free(err_msg);
      return false;
    }

    sql = "delete from version;";

    if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
      LOG_E(TAG, "SQL error (delete version rows): %s", err_msg);
      sqlite3_free(err_msg);
      return false;
    }

    sql = "insert into version (version, app_name) values (?,?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      LOG_E(TAG, "SQL error preparation of (insert into version): %s", sqlite3_errmsg(db));
      return false;
    }

    sqlite3_bind_int (stmt, 1, BOOKS_DIR_DB_VERSION);
    sqlite3_bind_text(stmt, 2, APP_NAME, strlen(APP_NAME), SQLITE_TRANSIENT);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

  }

  sql = 
    "create table if not exists books("
    "id           integer primary key,"
    "filename     text not null,"
    "file_size    int,"
    "title        text not null,"
    "author       text,"
    "description  text,"
    "cover_width  int,"
    "cover_height int,"
    "cover_bitmap blob,"
    "page_locs    blob);";

  if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
    LOG_E(TAG, "SQL error (create table books): %s", err_msg);
    sqlite3_free(err_msg);
    return false;
  }

  sql = "create index if not exists title_index on books(title)";
  
  if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
    LOG_E(TAG, "SQL error (create index): %s", err_msg);
    sqlite3_free(err_msg);
    return false;
  }

  refresh();

  sql = "select id from books order by title ASC;";
  stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_E(TAG, "SQL error (select id from books): %s", sqlite3_errmsg(db));
    return false;
  }

  book_ids.clear();

  rc = sqlite3_step(stmt);

  while (rc == SQLITE_ROW) {
    
    int64_t val = sqlite3_column_int(stmt, 0);
    book_ids.push_back(val);
    // LOG_D(TAG, "Retrieved Id: %d", val);

    rc = sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);

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

  if (idx >= book_ids.size()) return false;
  const char * sql = "select page_locs from books where id = ? limit 1;";
  sqlite3_stmt * stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_E(TAG, "SQL error (select id from books): %s", sqlite3_errmsg(db));
    return false;
  }

  if (sqlite3_bind_int64(stmt, 1, book_ids[idx]) != SQLITE_OK) {
    LOG_E(TAG, "Not able to bind id: %s", sqlite3_errmsg(db));
    return false;
  };

  if (sqlite3_step(stmt) != SQLITE_ROW) {
    LOG_E(TAG, "Could not step (select id from book) stmt: %s", sqlite3_errmsg(db));
    return false;
  }

  std::stringstream str;
  
  unsigned char * data = (unsigned char *) sqlite3_column_blob(stmt, 0);
  int32_t size = sqlite3_column_bytes(stmt, 0);
  
  str.write((const char *) data, size);

  deserialize(str, page_locs);

  sqlite3_finalize(stmt);

  return true;
}

const BooksDir::EBookRecord * 
BooksDir::get_book_data(int16_t idx)
{
  if (current_book_idx != idx) {
    if (idx >= book_ids.size()) return nullptr;
    const char * sql = "select filename, file_size, title, author, description, cover_width, cover_height, cover_bitmap from books where id = ? limit 1;";
    sqlite3_stmt * stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      LOG_E(TAG, "SQL error (select id from books): %s", sqlite3_errmsg(db));
      return nullptr;
    }

    if (sqlite3_bind_int64(stmt, 1, book_ids[idx]) != SQLITE_OK) {
      LOG_E(TAG, "Not able to bind id: %s", sqlite3_errmsg(db));
      return nullptr;
    };

    if (sqlite3_step(stmt) != SQLITE_ROW) {
      LOG_E(TAG, "Could not step (select id from book) stmt: %s", sqlite3_errmsg(db));
      return nullptr;
    }

    delete [] book.cover_bitmap;

    book.filename     = (const char *) sqlite3_column_text(stmt, 0);
    book.file_size    = sqlite3_column_int (stmt, 1);
    book.title        = (const char *) sqlite3_column_text(stmt, 2);
    book.author       = (const char *) sqlite3_column_text(stmt, 3);
    book.description  = (const char *) sqlite3_column_text(stmt, 4);
    book.cover_width  = sqlite3_column_int (stmt, 5);
    book.cover_height = sqlite3_column_int (stmt, 6);

    int16_t size = book.cover_width * book.cover_height;
    book.cover_bitmap = new unsigned char [size];

    unsigned char * bitmap = (unsigned char *) sqlite3_column_blob(stmt, 7);
    memcpy(book.cover_bitmap, bitmap, size);

    sqlite3_finalize(stmt);

    current_book_idx = idx;
  }
  return &book;
}

bool
BooksDir::refresh()
{
  //  First look if existing entries in the database exists as ebook.

  char * err_msg;
  const char * sql = "select id, filename, file_size from books;";
  sqlite3_stmt * stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_E(TAG, "SQL error (select id from books): %s", sqlite3_errmsg(db));
    return false;
  }

  book_ids.clear();

  int rc = sqlite3_step(stmt);
  
  std::vector<int64_t> to_be_deleted;
  to_be_deleted.clear();

  while (rc == SQLITE_ROW) {
    
    int64_t               id        = sqlite3_column_int (stmt, 0);
    const unsigned char * filename  = sqlite3_column_text(stmt, 1);
    int32_t               file_size = sqlite3_column_int (stmt, 2);

    std::string fname = BOOKS_FOLDER;
    fname.append((const char *) filename);

    struct stat stat_buffer;   
    if ((stat(fname.c_str(), &stat_buffer) != 0) || (stat_buffer.st_size != file_size)) {
      to_be_deleted.push_back(id);
    }

    rc = sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);(stmt);

  if (to_be_deleted.size() > 0) {

    sql = "delete from books where id = ?;";

    for (auto idx : to_be_deleted) {

      if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_E(TAG, "SQL error (delete from books): %s", sqlite3_errmsg(db));
        return false;
      }
      if (sqlite3_bind_int64(stmt, 1, idx) != SQLITE_OK) {
        LOG_E(TAG, "SQL error (sqlite3_bind idx): %s", sqlite3_errmsg(db));
        return false;
      }
      if (sqlite3_step(stmt) != SQLITE_DONE) {
        LOG_E(TAG, "Could not step (select id from book) stmt: %s", sqlite3_errmsg(db));
        return false;
      }
      sqlite3_finalize(stmt);
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

        int32_t count;
        std::string sql = "select count(*) from books where filename=? limit 1;";

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
          LOG_E(TAG, "SQL error (select count): %s", sqlite3_errmsg(db));
          return false;
        }

        if (sqlite3_bind_text(stmt, 1, de->d_name, -1, nullptr) != SQLITE_OK) {
          LOG_E(TAG, "Not able to bind filename: %s", sqlite3_errmsg(db));
          return false;
        };

        if (sqlite3_step(stmt) != SQLITE_ROW) {
          LOG_E(TAG, "Could not step (select id from book) stmt: %s", sqlite3_errmsg(db));
          return false;
        }

        count = sqlite3_column_int(stmt, 0);

        sqlite3_finalize(stmt);

        // LOG_D(TAG, "Check for: %s %d", de->d_name, count);

        if (count == 0) {

          // The book is not in the database, we add it now
          fname = BOOKS_FOLDER;
          fname.append(de->d_name);

          int32_t file_size = 0;
          struct stat stat_buffer;   
          if (stat(fname.c_str(), &stat_buffer) != 0) {
            LOG_E(TAG, "Unable to get stats for file: %s", fname.c_str());
          }
          else {
            file_size = stat_buffer.st_size;
          }
        
          if (epub.open_file(fname)) {
            EBookRecord    book;
            const char * str;

            book_view.build_page_locs(); // This build epub::page_locs vector
            std::stringstream streamed_page_locs;

            serialize(streamed_page_locs, epub.get_page_locs());

            book.filename     = de->d_name;
            book.file_size    = file_size;
            book.title.clear();
            book.author.clear();
            book.description.clear();
            book.cover_height = 0;
            book.cover_width  = 0;
            book.cover_bitmap = nullptr;

            if (str =       epub.get_title()) book.title       = str;
            if (str =      epub.get_author()) book.author      = str;
            if (str = epub.get_description()) book.description = str;

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

                book.cover_bitmap = new unsigned char[w * h];
                stbir_resize_uint8(image.bitmap, image.width, image.height, 0,
                                  book.cover_bitmap, w, h, 0,
                                  1);

                book.cover_width  = w;
                book.cover_height = h;
                stbi_image_free((void *) image.bitmap);
              }
            }
        
            sql = "insert into books (filename, file_size, title, author, description, cover_width, "
                  "cover_height, cover_bitmap, page_locs) values (?,?,?,?,?,?,?,?,?);";
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
              LOG_E(TAG, "SQL error (insert into books): %s", sqlite3_errmsg(db));
              return false;
            }
            sqlite3_bind_text(stmt, 1, de->d_name,               strlen(de->d_name),            SQLITE_TRANSIENT);
            sqlite3_bind_int (stmt, 2, book.file_size);
            sqlite3_bind_text(stmt, 3, book.title.c_str(),       book.title.size(),             SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, book.author.c_str(),      book.author.size(),            SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, book.description.c_str(), book.description.size(),       SQLITE_TRANSIENT);
            sqlite3_bind_int (stmt, 6, book.cover_width );
            sqlite3_bind_int (stmt, 7, book.cover_height);
            sqlite3_bind_blob(stmt, 8, book.cover_bitmap, book.cover_width * book.cover_height, SQLITE_TRANSIENT);
            sqlite3_bind_blob(stmt, 9, streamed_page_locs.str().c_str(), 
                                       streamed_page_locs.str().size(), SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
              LOG_E(TAG, "Could not step (insert into book) stmt: %s", sqlite3_errmsg(db));
              return false;
            }
            sqlite3_finalize(stmt);
            
            delete [] book.cover_bitmap;
            epub.close_file();
          }
        }
      }
    }
  }

  closedir(dp);

  return true;
}
