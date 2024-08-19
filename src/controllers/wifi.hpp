#pragma once

#include "global.hpp"

#if EPUB_INKPLATE_BUILD

#include "models/config.hpp"

#include "esp_wifi.h"

class WIFI
{
  protected:
    static constexpr char const * TAG = "WiFi";

    friend void wifi_sta_event_handler(void            * arg, 
                                       esp_event_base_t  event_base,
                                       int32_t           event_id, 
                                       void            * event_data);
    esp_ip4_addr_t ip_address;
    bool running;

    inline void set_ip_address(esp_ip4_addr_t addr) { ip_address = addr; }

  public:
    WIFI() : running(false) {}
    
    bool start();
    void stop();

    inline esp_ip4_addr_t get_ip_address()                    { return ip_address; }
};

#if __WIFI__
  WIFI wifi;
#else
  extern WIFI wifi;
#endif

#endif