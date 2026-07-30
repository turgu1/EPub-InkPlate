#pragma once

#include "global.hpp"

#if DATE_TIME_RTC

  #if EINK_BUILD
    #include "platform.hpp"
  #endif

  #include <sys/time.h>

  class Clock {
  private:
    static constexpr char const *TAG = "Clock";

  public:
    static auto setDateTime(const time_t &tm) -> void {
      #if EINK_BUILD
        if (platform.getRTC().isPresent()) {
          platform.getRTC().setDateTime(&tm);
        } else {
          timeval tv;
          tv.tv_sec  = tm;
          tv.tv_usec = 0;
          settimeofday(&tv, nullptr);
        }
      #endif
    }

    static auto getDateTime(time_t &t) -> void {
      #if EINK_BUILD
        if (platform.getRTC().isPresent()) {
          LOG_D("RTC chip is present");
          platform.getRTC().getDateTime(&t);
        } else {
          LOG_D("RTC chip is NOT present");
          time(&t);
        }
      #else
        time(&t);
      #endif
    }
  };

#endif