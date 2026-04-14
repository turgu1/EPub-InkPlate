#pragma once

#include "global.hpp"

#if EPUB_INKPLATE_BUILD

  #include "models/config.hpp"

  #include "esp_wifi.h"

  class WIFI {
  protected:
    static constexpr char const *TAG = "WiFi";

    friend void wifi_sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                       void *event_data);

    friend void wifi_ap_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                      void *event_data);
    esp_ip4_addr_t ipAddress;
    bool staRunning{false};
    bool apRunning{false};
    bool mdnsRunning{false};

    auto startMdnsService(const std::string &hostname) -> void;
 inline auto setIpAddress(esp_ip4_addr_t addr) -> void { ipAddress = addr; }

  public:
    WIFI() {}

    auto startSta() -> bool;
    auto startAp() -> bool;
    auto stop() -> void;

    inline esp_ip4_addr_t getIpAddress() { return ipAddress; }
  };

  #if __WIFI__
    WIFI wifi;
  #else
    extern WIFI wifi;
  #endif

#endif