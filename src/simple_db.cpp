#define __SIMPLE_DB__ 1
#include "simple_db.hpp"

#include "logging.hpp"

bool 
SimpleDB::open(std::string filename) 
{
  LOG_D("Opening database file: %s", filename.c_str());

  if (db_is_open) {
    fclose(db_file);
    db_is_open = false;
  }

  if ((db_file = fopen(filename.c_str(), "r+")) == nullptr) return create(filename);

  bool done = false;
  while (true) {
    if (fseek(db_file, 0, SEEK_END)) break;
    file_size = ftell(db_file);
    if (fseek(db_file, 0, SEEK_SET)) break;

    uint16_t idx    = 0;
    int32_t  offset = 0;
  
    while ((idx < MAX_RECORD_COUNT) && (offset < file_size)) {
      record_offset[idx] = offset;
      is_deleted[idx]    = false;
      int32_t size;
      if (fread(&size, sizeof(int32_t), 1, db_file) != 1) goto error;
      offset += size + sizeof(int32_t);
      if (offset < file_size) if (fseek(db_file, offset, SEEK_SET)) goto error;
      idx++;
    }
    done = true;
    record_count = idx;
    break;
  }
error:
  if (!done) {
    fclose(db_file);
    LOG_E("Database error!!");
    return false;
  }
  else {
    db_is_open          = true;
    some_record_deleted = false;
    current_record_idx  = 0;
    LOG_D("Record count: %d", record_count);
    return true;
  }
}

bool 
SimpleDB::create(std::string filename) 
{
  LOG_D("Creating database file: %s", filename.c_str());
  if (db_is_open) {
    fclose(db_file);
    db_is_open = false;
  }

  if ((db_file = fopen(filename.c_str(), "w+")) == nullptr) return false;

  db_is_open          = true;
  some_record_deleted = false;
  current_record_idx  = 0;
  record_count        = 0;
  file_size           = 0;

  return true;
}

void 
SimpleDB::close() 
{ 
  if (db_is_open) {
    db_is_open = false;
    fclose(db_file);
  }
}

bool 
SimpleDB::add_record(void * record, int32_t size) 
{
  LOG_D("Adding record of size %d", size);

  if (record_count >= MAX_RECORD_COUNT) return false;
  if (fseek(db_file, 0, SEEK_END)) return false;
  record_offset[record_count] = ftell(db_file);
  is_deleted[record_count++] = false;
  if (fwrite(&size, sizeof(int32_t), 1, db_file) != 1) return false;
  if (fwrite(record, size, 1, db_file) != 1) return false;
  fflush(db_file);
  file_size += sizeof(int32_t) + size;
  return true;
}

bool 
SimpleDB::get_record(void * record, int32_t size) 
{
  // LOG_D("Reading record of size %d", size);

  if ((size <= 0) || (current_record_idx >= record_count)) return false;
  if (fseek(db_file, record_offset[current_record_idx] + sizeof(int32_t), SEEK_SET)) return false;
  if (fread(record, size, 1, db_file) != 1) return false;
  return true;
}

bool 
SimpleDB::get_partial_record(void * record, int32_t size, int32_t offset) 
{
  // LOG_D("Reading partial record of size %d at offset %d", size, offset);

  if ((size <= 0) || (current_record_idx >= record_count)) return false;
  if (fseek(db_file, record_offset[current_record_idx] + sizeof(int32_t) + offset, SEEK_SET)) return false;
  if (fread(record, size, 1, db_file) != 1) return false;
  return true;
}
