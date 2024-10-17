#pragma once

#if __WEB_SERVER__
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

#include <cstdint>

enum class WebServerMode : uint8_t { STA, AP };

PUBLIC bool start_web_server(WebServerMode server_mode = WebServerMode::STA);
PUBLIC void stop_web_server();

#undef PUBLIC