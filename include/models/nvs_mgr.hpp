#pragma once

#include "global.hpp"

#include "nvs_flash.h"

class NVSMgr
{
  public:
    NVSMgr() : initialized(false) {}
   ~NVSMgr() {}

    #pragma pack(push, 1)
    struct NVSData {
      int32_t  offset;
      int16_t  itemref_index;
      uint8_t  was_shown;
      uint8_t  filler1;
    };
    #pragma pack(pop)

    bool         setup();
    bool save_location(uint32_t   id, const NVSData & nvs_data);
    bool      get_last(uint32_t & id,       NVSData & nvs_data);
    bool  get_location(uint32_t   id,       NVSData & nvs_data);
    bool     id_exists(uint32_t   id                          );

  private:
    static constexpr char const * TAG            = "NVSMgr";
    static constexpr char const * NAMESPACE      = "EPUB-InkPlate";
    static constexpr char const * PARTITION_NAME = "nvs";
    const uint8_t MAX_ENTRIES = 10;

    #pragma pack(push, 1)
    struct NVSControl {
      uint32_t first_idx;
      uint32_t next_idx;
    };

    union SavedData {
      NVSData  nvs_data;
      uint64_t data;
    };

    union SavedControl {
      NVSControl nvs_control;
      uint64_t   data;
    };
    #pragma pack(pop)

    nvs_handle_t nvs_handle;
    bool         initialized;
    SavedControl saved_control;

    bool retrieve(uint32_t index, uint32_t & id,       NVSData & nvs_data);
    bool retrieve(uint32_t index,                      NVSData & nvs_data);
    bool     save(uint32_t index, uint32_t   id, const NVSData & nvs_data);
    bool   update(uint32_t index,                const NVSData & nvs_data);
    void   remove(uint32_t index);
    bool  find_id(uint32_t id, uint32_t & index);
    void     show();

    inline std::string bld_key(const char * prefix, uint32_t index) {
      char str[12]; 
      return std::string(prefix) + itoa(index, str, 10); 
    }

    inline bool exists(uint32_t index) {
      std::string key = bld_key("ID_", index);
      uint32_t id;
      return nvs_get_u32(nvs_handle, key.c_str(), &id) == ESP_OK;
    }

    inline uint32_t size() { 
      return saved_control.nvs_control.next_idx - saved_control.nvs_control.first_idx;
    }
};

#if __NVS_MGR__
  NVSMgr nvs_mgr;
#else
  extern NVSMgr nvs_mgr;
#endif
