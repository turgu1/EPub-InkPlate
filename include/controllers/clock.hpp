#pragma once

#include "global.hpp"

#if DATE_TIME_RTC

#if EPUB_INKPLATE_BUILD
  #include "inkplate_platform.hpp"
#endif

#include "logging.hpp"

#include <sys/time.h>

class Clock
{
  private:
    static constexpr char const * TAG = "Clock";

  public:
    static void set_date_time(const time_t & tm) {
      #if EPUB_INKPLATE_BUILD
        if (rtc.is_present()) {
          rtc.set_date_time(&tm);
        }
        else {
          timeval tv;
          tv.tv_sec = tm;
          tv.tv_usec = 0;
          settimeofday(&tv, nullptr);
        }
      #endif
    }

    static void get_date_time(time_t & t) {
      #if EPUB_INKPLATE_BUILD
        if (rtc.is_present()) {
          LOG_D("RTC chip is present");
          rtc.get_date_time(&t);
        }
        else {
          LOG_D("RTC chip is NOT present");
          time(&t);
        }
      #else
        time(&t);
      #endif        
    }
};

#endif