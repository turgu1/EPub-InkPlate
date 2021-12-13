#pragma once

#if INKPLATE_6PLUS

#include "global.hpp"
#include "inkplate_platform.hpp"

#include "driver/rtc_io.h"

// Class used to control the backlit capability of the Inkplate-6PLUS

static RTC_DATA_ATTR uint8_t lit_level;

class BackLit 
{
  private:
    static constexpr char const * TAG = "BackLit";

    const uint8_t DEFAULT_LIT_LEVEL = 20;
    const uint8_t     MAX_LIT_LEVEL = 63;

    bool     lit;
    int16_t  pinch_distance;

  public:
    BackLit() : lit(false), pinch_distance(0) {}

    bool setup() {
      if (lit_level > MAX_LIT_LEVEL) lit_level = DEFAULT_LIT_LEVEL;
      if (lit_level > 0) {
        front_light.enable();
        front_light.set_level(lit_level);
        lit = true;
      }
      else {
        front_light.disable();
        lit = false;
      }

      return true;
    }

    void adjust(int16_t value) {
      pinch_distance += value;
      int16_t val = (pinch_distance > 0) ? pinch_distance / 4 : 0;

      lit_level = (val > MAX_LIT_LEVEL) ? MAX_LIT_LEVEL : val;

      if (lit_level == 0) {
        if (lit) {
          lit = false;
          front_light.disable();
        }
      }
      else {
        if (!lit) {
          lit = true;
          front_light.enable();
        }
        front_light.set_level(lit_level);
        if (lit_level == MAX_LIT_LEVEL) pinch_distance = MAX_LIT_LEVEL * 4;
      }

      LOG_D("Adjust: %d, lit_level: %u", value, lit_level);
    }

    void turn_off() { lit = false; front_light.disable(); }
};

#if __BACK_LIT__
  BackLit back_lit;
#else
  extern BackLit back_lit;
#endif

#endif
