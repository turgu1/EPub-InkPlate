#pragma once

#include "global.hpp"

#if EPUB_INKPLATE_BUILD

class NTP 
{
  private:
    static constexpr char const * TAG = "NTP";

    #pragma pack(push, 1)
    struct NTPPacket {

      uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
                              // li.   Two bits.   Leap indicator.
                              // vn.   Three bits. Version number of the protocol.
                              // mode. Three bits. Client will pick mode 3 for client.

      uint8_t stratum;         // Eight bits. Stratum level of the local clock.
      uint8_t poll;            // Eight bits. Maximum interval between successive messages.
      uint8_t precision;       // Eight bits. Precision of the local clock.

      uint32_t rootDelay;      // 32 bits. Total round trip delay time.
      uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
      uint32_t refId;          // 32 bits. Reference clock identifier.

      uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
      uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

      uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
      uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

      uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
      uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

      uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
      uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

    } ntp_packet;              // Total: 384 bits or 48 bytes.
    #pragma pack(pop)

    bool error(const char * msg);

  public:

    bool get_and_set_time();
};

#if __NTP__
  NTP ntp;
#else
  extern NTP ntp;
#endif

#endif