#if DATE_TIME_RTC

#define __NTP__ 1
#include "controllers/ntp.hpp"

#if EPUB_INKPLATE_BUILD
  #include "controllers/wifi.hpp"
  #include "controllers/clock.hpp"
  #include "models/config.hpp"

  #include "lwip/sockets.h"
  #include <lwip/netdb.h>
#endif

bool NTP::error(const char * msg)
{
  LOG_E("%s", msg);

  #if EPUB_INKPLATE_BUILD
    wifi.stop();
  #endif

  return false;
}

bool NTP::get_and_set_time() 
{
  #if EPUB_INKPLATE_BUILD

    int sockfd;

    constexpr uint16_t NTP_PORT            =           123;
    constexpr uint64_t NTP_TIMESTAMP_DELTA = 2208988800ull;

    NTPPacket packet;

    std::string host_name;

    config.get(Config::Ident::NTP_SERVER, host_name);

    memset(&packet, 0, sizeof(NTPPacket));
    packet.li_vn_mode = 0b00011011; // li = 0, vn = 3, mode = 3

    if (wifi.start_sta()) {

      // Create a UDP socket, convert the host-name to an IP address, set the port number,
      // connect to the server, send the packet, and then read in the return packet.

      struct sockaddr_in  serv_addr; // Server address data structure.
      struct hostent    * server;    // Server data structure.

      sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create a UDP socket.

      if (sockfd < 0) return error("ERROR opening socket");

      server = gethostbyname(host_name.c_str()); // Convert URL to IP.

      if (server == NULL) return error("ERROR, no such host");

      // Zero out the server address structure.

      bzero((char *) &serv_addr, sizeof(serv_addr));

      serv_addr.sin_family = AF_INET;

      // Copy the server's IP address to the server address structure.

      bcopy((char *)server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);

      // Convert the port number integer to network big-endian style and save it to the server address structure.

      serv_addr.sin_port = htons(NTP_PORT);

      // Call up the server using its IP address and port number.

      if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) return error("ERROR connecting");

      // Send it the NTP packet it wants. If n == -1, it failed.

      if (write(sockfd, (char *) &packet, sizeof(ntp_packet)) < 0) return error("ERROR writing to socket");

      // Wait and receive the packet back from the server. If n == -1, it failed.

      if (read(sockfd, (char *) &packet, sizeof(ntp_packet)) < 0) return error("ERROR reading from socket");

      wifi.stop();

      // These two fields contain the time-stamp seconds as the packet left the NTP server.
      // The number of seconds correspond to the seconds passed since 1900.
      // ntohl() converts the bit/byte order from the network's to host's "endianness".

      packet.txTm_s = ntohl(packet.txTm_s); // Time-stamp seconds.
      packet.txTm_f = ntohl(packet.txTm_f); // Time-stamp fraction of a second.

      // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
      // Subtract 70 years worth of seconds from the seconds since 1900.
      // This leaves the seconds since the UNIX epoch of 1970.
      // (1900)------------------(1970)**************************************(Time Packet Left the Server)

      time_t time = (time_t) (packet.txTm_s - NTP_TIMESTAMP_DELTA);

      // Print the time we got from the server, accounting for local timezone and conversion from UTC time.

      LOG_I("Time: %s", ctime((const time_t *) &time));

      Clock::set_date_time(time);
    }
    else {
      return false;
    }
  #endif

  return true;
}

#endif