#pragma once

#if EPUB_INKPLATE_BUILD

#include "global.hpp"

#include "nvs_flash.h"

#include <map>

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

    bool         setup(bool       force_erase = false);
    bool save_location(uint32_t   id, const NVSData & nvs_data);
    bool      get_last(uint32_t & id,       NVSData & nvs_data);
    bool  get_location(uint32_t   id,       NVSData & nvs_data);
    bool     id_exists(uint32_t   id                          );
    int8_t     get_pos(uint32_t   id                          );
    bool         erase(uint32_t   id                          );

  private:
    static constexpr char const * TAG            = "NVSMgr";
    static constexpr char const * NAMESPACE      = "EPUB-InkPlate";
    static constexpr char const * PARTITION_NAME = "nvs";
    const uint8_t MAX_ENTRIES = 10;

    typedef std::map <uint32_t, uint32_t> TrackList;

    uint8_t   track_count;
    TrackList track_list;
    uint32_t  next_idx;

    #pragma pack(push, 1)
    union SavedData {
      NVSData  nvs_data;
      uint64_t data;
    };
    #pragma pack(pop)

    nvs_handle_t nvs_handle;
    bool         initialized;

    bool retrieve(uint32_t index,                      NVSData & nvs_data);
    bool     save(                uint32_t   id, const NVSData & nvs_data);
    bool   update(uint32_t index, uint32_t   id, const NVSData & nvs_data);
    void   remove(uint32_t index);
    bool  find_id(uint32_t id, uint32_t & index);
    void     show();

    inline std::string bld_key(const char * prefix, uint32_t index) {
      char str[12]; 
      return std::string(prefix) + int_to_str(index, str, 12); 
    }

    inline bool exists(uint32_t index) {
      TrackList::iterator it = track_list.find(index);
      return it != track_list.end();
    }

    inline uint32_t size() { 
      return track_count;
    }
};

#if __NVS_MGR__
  NVSMgr nvs_mgr;
#else
  extern NVSMgr nvs_mgr;
#endif

#endif