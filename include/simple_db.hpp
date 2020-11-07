#ifndef __SIMPLE_DB_HPP__
#define __SIMPLE_DB_HPP__

#include <cinttypes>
#include <cstdio>
#include <string>

/**
 * @brief Very simple database tool
 * 
 * A *very* simple, one table database tool. Each record is having a single size that can
 * be different from each other. 
 * 
 * The tool maintains the location of each record in an array in RAM. No
 * index beyond that. When a database file is open, the tool computes the
 * location of each record. All record in file are considered valid.
 * 
 * A record can be marked as deleted using the set_deleted() method. This
 * will only mark it as deleted in memory. A new database needs to be
 * created and filled with valid records by the application to get rid of
 * marked as deleted records.
 * 
 * No memory allocation is done in the tool. This is the responsability
 * of the calling application.
 * 
 * Each record is preceeded with its size in file. Nothing else is kept in file
 * then the data associated with the record.
 * 
 * (c) 2020, Guy Turcotte
 */

class SimpleDB
{
  private:
    static const uint16_t MAX_RECORD_COUNT = 100;

    FILE * file;

    bool     db_is_open;
    bool     some_record_deleted;
    int32_t  record_offset[MAX_RECORD_COUNT]; ///< record offset in file or -1 if deleted
    uint16_t record_count;
    uint32_t file_size;
    uint16_t current_record_idx; ///< Index of current record in record_offset

  public:
    SimpleDB() : db_is_open(false), record_count(0), current_record_idx(0) {};
   ~SimpleDB() { if (db_is_open) fclose(file); }

    /**
     * @brief Open an existing database file.
     * 
     * Once opened, the record_offsets vector is builts, scanning
     * the file to recover each record size. If the db file doesn't exists,
     * it will be created.
     * 
     * @param filename 
     * @return true The file has been opened.
     * @return false 
     */
    bool open(std::string filename);
    
    /**
     * @brief Create a new database.
     * 
     * The current database is closed and a new empty file is created.
     * If a file already exists, it will be overided.
     * 
     * @param filename 
     * @return true File created and database pointing at it.
     * @return false File already exists and not overrided. Nothing done.
     */
    bool create(std::string filename);

    void close();
    
    inline uint16_t             get_current_idx() { return current_record_idx;  }
    inline void    set_current_idx(int16_t index) { current_record_idx = index; }
    inline uint16_t            get_record_count() { return record_count;        }
    inline uint16_t               get_file_size() { return file_size;           }
    inline bool          is_some_record_deleted() { return some_record_deleted; }
    inline bool                      is_db_open() { return db_is_open;          }

    /**
     * @brief Add a record at the end of the file.
     * 
     * Does not change current location.
     * 
     * @param record 
     * @param size 
     * @return true Record has been added.
     * @return false Potential file access issue.
     */
    bool add_record(void * record, int32_t size);

    bool get_record(void * record, int32_t size);

    bool get_partial_record(void * record, int32_t size, int32_t offset);

    /**
     * @brief Get size of the current record.
     * 
     * Returns 0 if at end of the database
     */
    int32_t get_record_size() {
      if (current_record_idx >= record_count) return 0;
      if (current_record_idx == (record_count - 1)) {
        return file_size - record_offset[current_record_idx] - sizeof(int32_t);
      }
      return record_offset[current_record_idx + 1] - record_offset[current_record_idx] - sizeof(int32_t);
    }

    /**
     * @brief Set current record as deleted.
     * 
     * @return true cursor is pointing at next valid record.
     * @return false no more valid record
     */
    bool set_deleted() { 
      record_offset[current_record_idx] = -1;
      some_record_deleted = true; 
      return goto_next();
    }

    bool end_of_db() { return current_record_idx >= record_count; }

    bool goto_first() {
      uint16_t idx = 0;
      while ((idx < record_count) && (record_offset[idx] == -1)) idx++;
      if (idx < record_count) {
        current_record_idx = idx;
        return true;
      }
      return false;
    }

    bool goto_next() {
      uint16_t idx = current_record_idx + 1;
      while ((idx < record_count) && (record_offset[idx] == -1)) idx++;
      if (idx < record_count) {
        current_record_idx = idx;
        return true;
      }

      return false;
    }
};

#endif
