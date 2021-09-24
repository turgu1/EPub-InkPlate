#define __NVS_MGR__ 1
#include "models/nvs_mgr.hpp"

bool
NVSMgr::setup()
{
  bool erased = false;
  initialized = false;

  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
      LOG_I("Erasing NVS Partition... (Because of %s)", esp_err_to_name(err));
      if ((err = nvs_flash_erase()) == ESP_OK) {
        err    = nvs_flash_init();
        erased = true;
      }
    }
  } 
  if (err != ESP_OK) LOG_E("NVS Error: %s", esp_err_to_name(err));

  if ((err = nvs_open(NAMESPACE, NVS_READWRITE, &nvs_handle)) == ESP_OK) {
    if ((err = nvs_get_u64(nvs_handle, "CONTROL", &saved_control.data)) != ESP_OK) {
      if (!erased) {
        if (((err = nvs_flash_erase()) != ESP_OK) ||
            ((err = nvs_flash_init())  != ESP_OK)) {
          LOG_E("NVS Error: %s", esp_err_to_name(err));
        }
        else {
          nvs_open(NAMESPACE, NVS_READWRITE, &nvs_handle);
        }
      }

      saved_control.nvs_control = {
        .first_idx  = 0,
        .next_idx   = 0 };

      if (((err = nvs_set_u64(nvs_handle, "CONTROL", saved_control.data)) == ESP_OK) &&
          ((err =  nvs_commit(nvs_handle)                               ) == ESP_OK)) {
        initialized = true;
     }
      else {
        LOG_E("Unable to initialize nvs: %s", esp_err_to_name(err));
      }
    }
    else {
      initialized = true;
    }
    nvs_close(nvs_handle);
  }
  else {
    LOG_E("Unable to access nvs: %s.", esp_err_to_name(err));
  }

  #if DEBUGGING
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
  bool      control_modified = false;
  bool      result           = false;

  if ((err = nvs_open(NAMESPACE, NVS_READWRITE, &nvs_handle)) == ESP_OK) {
    if (find_id(id, index)) {
      result = update(index, nvs_data);
      #if DEBUGGING
        show();
      #endif
    }
    else {
      while (size() >= MAX_ENTRIES) {
        remove(saved_control.nvs_control.first_idx++);
        control_modified = true;
      }
      index = saved_control.nvs_control.next_idx;
      if (save(index, id, nvs_data)) {
        saved_control.nvs_control.next_idx++;
        control_modified = true;
      }
      if (control_modified) {
        if ((err = nvs_set_u64(nvs_handle, "CONTROL", saved_control.data)) != ESP_OK) {
          LOG_E("Unable to save new CONTROL content: %s", esp_err_to_name(err));
        }
        else {
          if (((err = nvs_set_u32(nvs_handle, "LAST_IDX", index)) == ESP_OK) &&
              ((err =  nvs_commit(nvs_handle)                   ) == ESP_OK)) {
            #if DEBUGGING
              show();
            #endif
            result = true;
          }
          else {
            LOG_E("Unable to save LAST_IDX: %s", esp_err_to_name(err));
          }
        }
      }
    }
    nvs_close(nvs_handle);
  }
  else {
    LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
  }

  return result;
}

bool
NVSMgr::get_last(uint32_t & id, NVSData & nvs_data)
{
  if (!initialized) return false;

  bool found = false;
  esp_err_t err;

  uint32_t index;

  if ((err = nvs_open(NAMESPACE, NVS_READONLY, &nvs_handle)) == ESP_OK) {
    if ((err = nvs_get_u32(nvs_handle, "LAST_IDX", &index)) == ESP_OK) {
      found = retrieve(index, id, nvs_data);
    }
    nvs_close(nvs_handle);
  }
  else {
    LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
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
NVSMgr::id_exists(uint32_t id)
{
  esp_err_t err;
  uint32_t index;
  bool found = false;

  if ((err = nvs_open(NAMESPACE, NVS_READONLY, &nvs_handle)) == ESP_OK) {
    found = find_id(id, index);
    nvs_close(nvs_handle);
  }

  return found;
}

bool
NVSMgr::retrieve(uint32_t index, uint32_t & id, NVSData & nvs_data)
{
  std::string key = bld_key("ID_", index);
  SavedData   saved_data;

  if (nvs_get_u32(nvs_handle, key.c_str(), &id) == ESP_OK) {
    key = bld_key("DATA_", index);
    if (nvs_get_u64(nvs_handle, key.c_str(), &saved_data.data) == ESP_OK) {
      nvs_data = saved_data.nvs_data;
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
NVSMgr::save(uint32_t index, uint32_t id, const NVSData & nvs_data)
{
  std::string key = bld_key("ID_", index);
  SavedData   saved_data;
  esp_err_t   err;

  saved_data.nvs_data = nvs_data;
  if ((err = nvs_set_u32(nvs_handle, key.c_str(), id)) == ESP_OK) {
    key = bld_key("DATA_", index);
    if ((err = nvs_set_u64(nvs_handle, key.c_str(), saved_data.data)) == ESP_OK) {
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
NVSMgr::update(uint32_t index, const NVSData & nvs_data)
{
  std::string key = bld_key("DATA_", index);
  SavedData   saved_data;
  esp_err_t   err;
  uint64_t    old_data;

  if ((err = nvs_get_u64(nvs_handle, key.c_str(), &old_data)) == ESP_OK) {
    saved_data.nvs_data = nvs_data;
    if (saved_data.data != old_data) {
      if ((err = nvs_set_u64(nvs_handle, key.c_str(), saved_data.data)) == ESP_OK) {
        return true;
      }
      else {
        key = bld_key("ID_", index);
        nvs_erase_key(nvs_handle, key.c_str());
      }
    }
  }
  else {
    key = bld_key("ID_", index);
    nvs_erase_key(nvs_handle, key.c_str());
  }

  LOG_E("Unable to update entry: %s", esp_err_to_name(err));
  return false;
}

void 
NVSMgr::remove(uint32_t index)
{
  if (exists(index)) {
    std::string key = bld_key("ID_", index);
    nvs_erase_key(nvs_handle, key.c_str());
    key = bld_key("DATA_", index);
    nvs_erase_key(nvs_handle, key.c_str());
  }
}

bool
NVSMgr::find_id(uint32_t id, uint32_t & index)
{
  uint32_t  the_id;

  for (uint32_t idx = saved_control.nvs_control.first_idx; 
       idx < saved_control.nvs_control.next_idx; 
       idx++) {
    std::string key = bld_key("ID_", idx);
    if ((nvs_get_u32(nvs_handle, key.c_str(), &the_id) == ESP_OK) && (the_id == id)) {
      index = idx;
      return true;
    }
  }

  return false;
}

#if DEBUGGING
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
      SavedControl s_control;
      if (nvs_get_u64(nvs_handle, "CONTROL", &s_control.data) == ESP_OK) {
        std::cout << "  CONTROL: first_idx: " 
                  << s_control.nvs_control.first_idx
                  << " next_idx: "
                  << s_control.nvs_control.next_idx << std::endl;
      }

      uint32_t index;
      if (nvs_get_u32(nvs_handle, "LAST_IDX", &index) == ESP_OK) {
        std::cout << "  LAST_IDX: " << index << std::endl;
      }

      SavedData  s_data;
      uint32_t   id;
      for (uint32_t idx = s_control.nvs_control.first_idx;
                    idx < s_control.nvs_control.next_idx;
                    idx++) {
        std::string key = bld_key("ID_", idx);
        if (nvs_get_u32(nvs_handle, key.c_str(), &id) == ESP_OK) {
          std::cout << "  " << key << ": " << id << std::endl;
        }
        key = bld_key("DATA_", idx);
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