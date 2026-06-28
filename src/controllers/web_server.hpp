#pragma once

#if __WEB_SERVER__
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

#include <cstdint>

enum class WebServerMode : uint8_t {
  STA, AP
};

PUBLIC auto startWebServer(WebServerMode serverMode = WebServerMode::STA) -> bool;
PUBLIC auto startWebServerHeadless(WebServerMode serverMode = WebServerMode::STA) -> bool;
PUBLIC void stopWebServer();

#undef PUBLIC