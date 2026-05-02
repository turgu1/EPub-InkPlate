#if EPUB_INKPLATE_BUILD

  #define __NVS_MGR__ 1
  #include "models/nvs_mgr.hpp"
  #include "models/books_dir.hpp"
  auto NVSMgr::setup(bool forceErase) -> bool {
    bool erased = false;
    esp_err_t err;

    initialized = false;
    trackCount  = 0;
    nextIdx     = 0;

    trackList.clear();

    if (forceErase) {
      if ((err = nvs_flash_erase()) == ESP_OK) {
        err    = nvs_flash_init();
        erased = true;
      }
    } else {
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

    if ((err = nvs_open(NAMESPACE, NVS_READWRITE, &nvsHandle)) == ESP_OK) {
      if ((err = nvs_get_u32(nvsHandle, "NEXT_IDX", &nextIdx)) != ESP_OK) {
        if (!erased) {
          if (((err = nvs_flash_erase()) != ESP_OK) || ((err = nvs_flash_init()) != ESP_OK)) {
            LOG_E("NVS Error: %s", esp_err_to_name(err));
          } else {
            nvs_open(NAMESPACE, NVS_READWRITE, &nvsHandle);
          }
        }

        nextIdx = 0;
        if (((err = nvs_set_u32(nvsHandle, "NEXT_IDX", nextIdx)) == ESP_OK) &&
            ((err = nvs_commit(nvsHandle)) == ESP_OK)) {
          initialized = true;
        } else {
          LOG_E("Unable to initialize nvs: %s", esp_err_to_name(err));
        }
      } else {
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
            if (nvs_get_u32(nvsHandle, info.key, &id) == ESP_OK) {
              trackList[index] = id;
              ++trackCount;
            }
          }
          res = nvs_entry_next(&it);
        };
        nvs_release_iterator(it);
      }
      nvs_close(nvsHandle);
    } else {
      LOG_E("Unable to access nvs: %s.", esp_err_to_name(err));
    }

  #if 0 //DEBUGGING
    show();
    #endif

    return initialized;
  }

  auto NVSMgr::saveLocation(uint32_t id, const NVSData &nvsData) -> bool {
    if (!initialized) return false;

    esp_err_t err;
    uint32_t index;
    bool result = false;

    if ((err = nvs_open(NAMESPACE, NVS_READWRITE, &nvsHandle)) == ESP_OK) {
      if (findId(id, index)) {
        result = update(index, id, nvsData);
      } else {
        result = save(id, nvsData);
      }
      nvs_commit(nvsHandle);
      nvs_close(nvsHandle);
    } else {
      LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
    }

  #if 0 // DEBUGGING
    show();
    #endif

    return result;
  }

  auto NVSMgr::getLast(uint32_t &id, NVSData &nvsData) -> bool {
    if (!initialized) return false;

    bool found = false;
    esp_err_t err;

    TrackList::reverse_iterator it = trackList.rbegin();

    if (it != trackList.rend()) {
      id = it->second;

      if ((err = nvs_open(NAMESPACE, NVS_READONLY, &nvsHandle)) == ESP_OK) {
        found = retrieve(it->first, nvsData);
        nvs_close(nvsHandle);
      } else {
        LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
      }
    }
    return found;
  }

  auto NVSMgr::getLocation(uint32_t id, NVSData &nvsData) -> bool {
    if (!initialized) return false;

    esp_err_t err;
    uint32_t index;
    bool found = false;

    if ((err = nvs_open(NAMESPACE, NVS_READONLY, &nvsHandle)) == ESP_OK) {
      if (findId(id, index)) {
        found = retrieve(index, nvsData);
      }
      nvs_close(nvsHandle);
    } else {
      LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
    }

    return found;
  }

  auto NVSMgr::erase(uint32_t id) -> bool {
    if (!initialized) return false;

    esp_err_t err;
    uint32_t index;

    if ((err = nvs_open(NAMESPACE, NVS_READONLY, &nvsHandle)) == ESP_OK) {
      if (findId(id, index)) {
        remove(index);
        return true;
      }
      nvs_close(nvsHandle);
    } else {
      LOG_E("Unable to open NVS: %s", esp_err_to_name(err));
    }
    return false;
  }

  auto NVSMgr::idExists(uint32_t id) -> bool {
    for (auto &e : trackList) {
      if (e.second == id) {
        return true;
      }
    }
    return false;
  }

  auto NVSMgr::retrieve(uint32_t index, NVSData &nvsData) -> bool {
    std::string key = bldKey("DATA_", index);
    SavedData savedData;

    if (nvs_get_u64(nvsHandle, key.c_str(), &savedData.data) == ESP_OK) {
      nvsData = savedData.nvsData;
      return true;
    }

    return false;
  }

  auto NVSMgr::save(uint32_t id, const NVSData &nvsData) -> bool {
    SavedData savedData;
    esp_err_t err;

    while (trackCount >= 10) {
      TrackList::iterator it = trackList.begin();
      if (it != trackList.end()) remove(it->first);
    }

    savedData.nvsData = nvsData;
    uint32_t index    = nextIdx;
    std::string key   = bldKey("ID_", index);

    if ((err = nvs_set_u32(nvsHandle, key.c_str(), id)) == ESP_OK) {
      key = bldKey("DATA_", index);
      if ((err = nvs_set_u64(nvsHandle, key.c_str(), savedData.data)) == ESP_OK) {
        if ((err = nvs_set_u32(nvsHandle, "NEXT_IDX", ++nextIdx)) != ESP_OK) {
          LOG_E("Unable to save new NEXT_IDX: %s", esp_err_to_name(err));
        }
        trackList[index] = id;
        ++trackCount;
        int8_t pos = 0;
        for (TrackList::reverse_iterator rit = trackList.rbegin(); rit != trackList.rend();
             ++rit, ++pos) {
          booksDir.setTrackOrder(rit->second, pos);
        }
        return true;
      } else {
        key = bldKey("ID_", index);
        nvs_erase_key(nvsHandle, key.c_str());
      }
    }

    LOG_E("Unable to save new entry: %s", esp_err_to_name(err));
    return false;
  }

  auto NVSMgr::update(uint32_t index, uint32_t id, const NVSData &nvsData) -> bool {
    TrackList::reverse_iterator rit = trackList.rbegin();
    if ((rit != trackList.rend()) && (index == (rit->first))) {
      uint64_t oldData;
      SavedData newData;
      newData.nvsData = nvsData;
      std::string key = bldKey("DATA_", index);
      if ((nvs_get_u64(nvsHandle, key.c_str(), &oldData) == ESP_OK) && (oldData == newData.data)) {
        return true;
      }
    }
    remove(index);
    return save(id, nvsData);
  }

  auto NVSMgr::remove(uint32_t index) -> void {
    if (exists(index)) {
      std::string key = bldKey("ID_", index);
      uint32_t theId;
      if (nvs_get_u32(nvsHandle, key.c_str(), &theId) == ESP_OK) {
        booksDir.setTrackOrder(theId, -1);
      }
      nvs_erase_key(nvsHandle, key.c_str());
      key = bldKey("DATA_", index);
      nvs_erase_key(nvsHandle, key.c_str());
      if (trackCount > 0) {
        trackList.erase(index);
        trackCount--;
      }
    }
  }

  auto NVSMgr::findId(uint32_t id, uint32_t &index) -> bool {
    for (auto &e : trackList) {
      if (e.second == id) {
        index = e.first;
        return true;
      }
    }

    return false;
  }

  auto NVSMgr::getPos(uint32_t id) -> int8_t {
    int8_t pos = 0;
    bool found = false;

    for (TrackList::reverse_iterator rit = trackList.rbegin(); rit != trackList.rend();
         ++rit, ++pos) {
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
    nvs_stats_t nvsStats;
    nvs_get_stats(NULL, &nvsStats);

    std::cout << "===== NVS Info =====" << std::endl
              << "Stats:" << std::endl
              << "    UsedEntries: " << nvsStats.used_entries  << std::endl 
              << "    FreeEntries: " << nvsStats.free_entries  << std::endl
              << "    AllEntries: "  << nvsStats.total_entries << std::endl;

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
    if (nvs_open(NAMESPACE, NVS_READONLY, &nvsHandle) == ESP_OK) {
      uint32_t index;
      if (nvs_get_u32(nvsHandle, "NEXT_IDX", &index) == ESP_OK) {
        std::cout << "  NEXT_IDX: " << index << std::endl;
      }

      SavedData sData;

      std::cout << "  Track count: " << +trackCount << std::endl;

      for (TrackList::reverse_iterator rit = trackList.rbegin();
                    rit != trackList.rend();
                    ++rit) {
        std::string key = bldKey("ID_", rit->first);
        uint32_t    id;
        if (nvs_get_u32(nvsHandle, key.c_str(), &id) == ESP_OK) {
          std::cout << "  " << key << ": " << rit->second << std::endl;
          if (id != rit->second) {
            LOG_E("THE IS IS NOT CONFORM (%" PRIu32 " vs %" PRIu32 ")", id, rit->second);
          }
        }
        key = bldKey("DATA_", rit->first);
        if (nvs_get_u64(nvsHandle, key.c_str(), &sData.data) == ESP_OK) {
          std::cout << "  " << key << ": offset: "
                    << sData.nvsData.offset
                    << " itemrefIndex: " 
                    << sData.nvsData.itemrefIndex
                    << " wasShown: "
                    << +sData.nvsData.wasShown << std::endl;
        }
      }
    }
    std::cout << "===== END NVS =====" << std::endl;
  }
  #endif

#endif