#pragma once

#if EPUB_INKPLATE_BUILD

  #include "global.hpp"

  #include "nvs_flash.h"

  #include <map>
  #include <string>

  class NVSMgr {
  public:
    NVSMgr() : trackCount(0), nextIdx(0), initialized(false) {}
    ~NVSMgr() {}

    #pragma pack(push, 1)
    struct NVSData {
      int32_t offset;
      int16_t itemrefIndex;
      uint8_t wasShown;
      uint8_t filler;
    };
    #pragma pack(pop)

    auto setup(bool forceErase = false) -> bool;
    auto saveLocation(uint32_t id, const NVSData &nvsData) -> bool;
    auto getLast(uint32_t &id, NVSData &nvsData) -> bool;
    auto getLocation(uint32_t id, NVSData &nvsData) -> bool;
    auto idExists(uint32_t id) -> bool;
    auto getPos(uint32_t id) -> int8_t;
    auto erase(uint32_t id) -> bool;

  private:
    static constexpr char const *TAG            = "NVSMgr";
    static constexpr char const *NAMESPACE      = "EPUB-InkPlate";
    static constexpr char const *PARTITION_NAME = "nvs";
    const uint8_t MAX_ENTRIES                   = 10;

    using TrackList = std::map<uint32_t, uint32_t>;

    uint8_t trackCount;
    TrackList trackList;
    uint32_t nextIdx;

    #pragma pack(push, 1)
    union SavedData {
      NVSData nvsData;
      uint64_t data;
    };
    #pragma pack(pop)

    nvs_handle_t nvsHandle;
    bool initialized;

    auto retrieve(uint32_t index, NVSData &nvsData) -> bool;
    auto save(uint32_t id, const NVSData &nvsData) -> bool;
    auto update(uint32_t index, uint32_t id, const NVSData &nvsData) -> bool;
    auto remove(uint32_t index) -> void;
    auto findId(uint32_t id, uint32_t &index) -> bool;
    auto show() -> void;

    [[nodiscard]] inline auto bldKey(const char *prefix, uint32_t index) -> std::string {
      char str[12];
      return std::string(prefix) + int_to_str(index, str, 12);
    }

    [[nodiscard]] inline auto exists(uint32_t index) -> bool {
      TrackList::iterator it = trackList.find(index);
      return it != trackList.end();
    }

    [[nodiscard]] inline auto size() -> uint32_t { return trackCount; }
  };

  #if __NVS_MGR__
    NVSMgr nvsMgr;
  #else
    extern NVSMgr nvsMgr;
  #endif

#endif