#if EPUB_INKPLATE_BUILD

#define __NVS_MGR__ 1
#include "models/nvs_mgr.hpp"
#include "models/books_dir.hpp"
bool
NVSMgr::setup(bool force_erase)
{
  bool erased = false;
  esp_err_t err;

  initialized = false;
  track_count = 0;
  next_idx    = 0;
  
  track_list.clear();

  if (force_erase) {
    if ((err = nvs_flash_erase()) == ESP_OK) {
      err    = nvs_flash_init();
      erased = true;
    }
  }
  else {
    err = nvs_flash_init();
    if (err != ESP_OK) {
      if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        LOG_I("Erasing NVS Partition... (Because of %s)", esp_err_to_name(err));
        if ((err = nvs_flash_erase()) == ESP_OK) {
          err    = nvs_flash_init();
          erased = true;
        }
      }
    }
  } 
  if (err != ESP_OK) LOG_E("NVS Error: %s", esp_err_to_name(err));

  if ((err = nvs_open(NAMESPACE, NVS_READWRITE, &nvs_handle)) == ESP_OK) {
    if ((err = nvs_get_u32(nvs_handle, "NEXT_IDX", &next_idx)) != ESP_OK) {
      if (!erased) {
        if (((err = nvs_flash_erase()) != ESP_OK) ||
            ((err = nvs_flash_init())  != ESP_OK)) {
          LOG_E("NVS Error: %s", esp_err_to_name(err));
        }
        else {
          nvs_open(NAMESPACE, NVS_READWRITE, &nvs_handle);
        }
      }

      next_idx = 0;
      if (((err = nvs_set_u32(nvs_handle, "NEXT_IDX", next_idx)) == ESP_OK) &&
          ((err =  nvs_commit(nvs_handle)                      ) == ESP_OK)) {
        initialized = true;
     }
      else {
        LOG_E("Unable to initialize nvs: %s", esp_err_to_name(err));
      }
    }
    else {
      initialized = true;
    }
    
    if (initialized) {
      nvs_iterator_t it;
      esp_err_t res = nvs_entry_find(PARTITION_NAME, NAMESPACE, NVS_TYPE_ANY, &it);
      while (res == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        if (strncmp(info.key, "ID_", 3) == 0) {
          uint32_t index = atoi(&info.key[3]);
          uint32_t id;
          if (nvs_get_u32(nvs_handle, info.key, &id) == ESP_OK) {
            track_list[index] = id;
            track_count++;
          }
        }
        res = nvs_entry_next(&it);
      };
      nvs_release_iterator(it);
    }
    nvs_close(nvs_handle);
  }
  else {
    LOG_E("Unable to access nvs: %s.", esp_err_to_name(err));
  }

  #if 0 //DEBUGGING
    show();
  #endif

  return initialized;
}

bool
NVSMgr::save_location(uint32_t id, const NVSData & nvs_data)
{
  if (!initialized) return false;

  esp_err_t err;
  uint32_t  index;
  bool      result = false;

  if ((err = nvs_open(NAMESPACE, NVS_READWRITE, &nvs_handle)) == ESP_OK) {
    if (find_id(id, index)) {
      result = update(index, id, nvs_data);
    }
    else {
      result = save(id, nvs_data);
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
  }
  else {
    LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
  }

  #if 0 // DEBUGGING
    show();
  #endif

  return result;
}

bool
NVSMgr::get_last(uint32_t & id, NVSData & nvs_data)
{
  if (!initialized) return false;

  bool found = false;
  esp_err_t err;

  TrackList::reverse_iterator it = track_list.rbegin();

  if (it != track_list.rend()) {
    id = it->second;

    if ((err = nvs_open(NAMESPACE, NVS_READONLY, &nvs_handle)) == ESP_OK) {
      found = retrieve(it->first, nvs_data);
      nvs_close(nvs_handle);
    }
    else {
      LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
    }
  }
  return found;
}

bool
NVSMgr::get_location(uint32_t id, NVSData & nvs_data)
{
  if (!initialized) return false;

  esp_err_t err;
  uint32_t index;
  bool found = false;

  if ((err = nvs_open(NAMESPACE, NVS_READONLY, &nvs_handle)) == ESP_OK) {
    if (find_id(id, index)) {
      found = retrieve(index, nvs_data);
    }
    nvs_close(nvs_handle);
  }
  else {
    LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
  }

  return found;
}

bool
NVSMgr::erase(uint32_t id ) 
{
  if (!initialized) return false;

  esp_err_t err;
  uint32_t index;

  if ((err = nvs_open(NAMESPACE, NVS_READONLY, &nvs_handle)) == ESP_OK) {
    if (find_id(id, index)) {
      remove(index);
      return true;
    }
    nvs_close(nvs_handle);
  }
  else {
    LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
  }
  return false;
}

bool
NVSMgr::id_exists(uint32_t id)
{
  for (auto & e : track_list) {
    if (e.second == id) {
      return true;
    }
  }
  return false;
}

bool
NVSMgr::retrieve(uint32_t index, NVSData & nvs_data)
{
  std::string key = bld_key("DATA_", index);
  SavedData   saved_data;

  if (nvs_get_u64(nvs_handle, key.c_str(), &saved_data.data) == ESP_OK) {
    nvs_data = saved_data.nvs_data;
    return true;
  }

  return false;
}

bool
NVSMgr::save(uint32_t id, const NVSData & nvs_data)
{
  SavedData   saved_data;
  esp_err_t   err;

  while (track_count >= 10) {
    TrackList::iterator it = track_list.begin();
    if (it != track_list.end()) remove(it->first);
  }

  saved_data.nvs_data = nvs_data;
  uint32_t      index = next_idx;
  std::string     key = bld_key("ID_", index);

  if ((err = nvs_set_u32(nvs_handle, key.c_str(), id)) == ESP_OK) {
    key = bld_key("DATA_", index);
    if ((err = nvs_set_u64(nvs_handle, key.c_str(), saved_data.data)) == ESP_OK) {
      if ((err = nvs_set_u32(nvs_handle, "NEXT_IDX", ++next_idx)) != ESP_OK) {
        LOG_E("Unable to save new NEXT_IDX: %s", esp_err_to_name(err));
      }
      track_list[index] = id;
      track_count++;
      int8_t pos = 0;
      for (TrackList::reverse_iterator rit = track_list.rbegin(); 
           rit != track_list.rend(); 
           rit++, pos++) {
        books_dir.set_track_order(rit->second, pos);
      }
      return true;
    }
    else {
      key = bld_key("ID_", index);
      nvs_erase_key(nvs_handle, key.c_str());
    }
  }

  LOG_E("Unable to save new entry: %s", esp_err_to_name(err));
  return false;
}

bool
NVSMgr::update(uint32_t index, uint32_t id, const NVSData & nvs_data)
{
  TrackList::reverse_iterator rit = track_list.rbegin();
  if ((rit != track_list.rend()) && (index == (rit->first))) {
    uint64_t  old_data;
    SavedData new_data;
    new_data.nvs_data = nvs_data;
    std::string   key = bld_key("DATA_", index);
    if ((nvs_get_u64(nvs_handle, key.c_str(), &old_data) == ESP_OK) &&
        (old_data == new_data.data)) {
      return true;
    }
  }
  remove(index);
  return save(id, nvs_data);
}

void 
NVSMgr::remove(uint32_t index)
{
  if (exists(index)) {
    std::string key = bld_key("ID_", index);
    uint32_t the_id;
    if (nvs_get_u32(nvs_handle, key.c_str(), &the_id) == ESP_OK) {
      books_dir.set_track_order(the_id, -1);
    }
    nvs_erase_key(nvs_handle, key.c_str());
    key = bld_key("DATA_", index);
    nvs_erase_key(nvs_handle, key.c_str());
    if (track_count > 0) {
      track_list.erase(index);
      track_count--;
    }
  }
}

bool
NVSMgr::find_id(uint32_t id, uint32_t & index)
{
  for (auto & e : track_list) {
    if (e.second == id) {
      index = e.first;
      return true;
    }
  }

  return false;
}

int8_t NVSMgr::get_pos(uint32_t id)
{
  int8_t pos = 0;
  bool found = false;

  for (TrackList::reverse_iterator rit = track_list.rbegin();
       rit != track_list.rend();
       rit++, pos++) {
    if (rit->second == id) {
      found = true;
      break;
    }
  }

  return found ? pos : -1;
}

#if 0 //DEBUGGING
  void
  NVSMgr::show()
  {
    nvs_stats_t nvs_stats;
    nvs_get_stats(NULL, &nvs_stats);

    std::cout << "===== NVS Info =====" << std::endl
              << "Stats:" << std::endl
              << "    UsedEntries: " << nvs_stats.used_entries  << std::endl 
              << "    FreeEntries: " << nvs_stats.free_entries  << std::endl
              << "    AllEntries: "  << nvs_stats.total_entries << std::endl;

    nvs_iterator_t it = nvs_entry_find(PARTITION_NAME, NAMESPACE, NVS_TYPE_ANY);
    std::cout << "Content:" << std::endl;
    while (it != NULL) {
      nvs_entry_info_t info;
      nvs_entry_info(it, &info);
      it = nvs_entry_next(it);
      std::cout << "    key: "  << info.key
                <<    " type: " << info.type << std::endl;
    };
    nvs_release_iterator(it);

    std::cout << NAMESPACE << ":" << std::endl;
    if (nvs_open(NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
      uint32_t index;
      if (nvs_get_u32(nvs_handle, "NEXT_IDX", &index) == ESP_OK) {
        std::cout << "  NEXT_IDX: " << index << std::endl;
      }

      SavedData s_data;

      std::cout << "  Track count: " << +track_count << std::endl;

      for (TrackList::reverse_iterator rit = track_list.rbegin();
                    rit != track_list.rend();
                    rit++) {
        std::string key = bld_key("ID_", rit->first);
        uint32_t    id;
        if (nvs_get_u32(nvs_handle, key.c_str(), &id) == ESP_OK) {
          std::cout << "  " << key << ": " << rit->second << std::endl;
          if (id != rit->second) {
            LOG_E("THE IS IS NOT CONFORM (%" PRIu32 " vs %" PRIu32 ")", id, rit->second);
          }
        }
        key = bld_key("DATA_", rit->first);
        if (nvs_get_u64(nvs_handle, key.c_str(), &s_data.data) == ESP_OK) {
          std::cout << "  " << key << ": offset: "
                    << s_data.nvs_data.offset
                    << " itemref_index: " 
                    << s_data.nvs_data.itemref_index
                    << " was_shown: "
                    << +s_data.nvs_data.was_shown << std::endl;
        }
      }
    }
    std::cout << "===== END NVS =====" << std::endl;
  }
#endif

#endif