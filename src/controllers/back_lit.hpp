#pragma once

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK

  #include "global.hpp"
  #include "inkplate_platform.hpp"

  #include "driver/rtc_io.h"
  #include "rtc_pcf85063.hpp"

  // Class used to control the backlit capability of the Inkplate-6PLUS

  // static RTC_DATA_ATTR uint8_t saved_lit_level;

  class BackLit {
  private:
    static constexpr char const *TAG = "BackLit";

    const uint8_t DEFAULT_LIT_LEVEL = 20;
    const uint8_t MAX_LIT_LEVEL     = 63;

    bool lit;
    int16_t pinchDistance;

    // uint8_t getLitLevel() { return saved_lit_level; }
    // void    saveLitLevel(uint8_t level) { saved_lit_level = level; }

    auto getLitLevel() -> uint8_t { return rtc.get_ram(); }
    auto saveLitLevel(uint8_t level) -> void { rtc.set_ram(level); }

  public:
    BackLit() : lit(false), pinchDistance(0) {}

    auto setup() -> bool {
      uint8_t litLevel = getLitLevel();
      if (litLevel > MAX_LIT_LEVEL) litLevel = DEFAULT_LIT_LEVEL;
      if (litLevel > 0) {
        front_light.enable();
        front_light.set_level(litLevel);
        pinchDistance = litLevel * 4;
        lit           = true;
      } else {
        front_light.disable();
        lit = false;
      }
      saveLitLevel(litLevel);

      return true;
    }

    auto adjust(int16_t value) -> void {
      pinchDistance += value;
      int16_t val = (pinchDistance > 0) ? pinchDistance / 4 : 0;

      uint8_t litLevel = (val > MAX_LIT_LEVEL) ? MAX_LIT_LEVEL : val;

      if (litLevel == 0) {
        if (lit) {
          lit = false;
          front_light.disable();
        }
      } else {
        if (!lit) {
          lit = true;
          front_light.enable();
        }
        front_light.set_level(litLevel);
        if (litLevel == MAX_LIT_LEVEL) pinchDistance = MAX_LIT_LEVEL * 4;
      }
      saveLitLevel(litLevel);

      LOG_D("Adjust: %d, litLevel: %u", value, litLevel);
    }

    auto turnOff() -> void {
      lit = false;
      front_light.disable();
    }
  };

  #if __BACK_LIT__
    BackLit backLit;
  #else
    extern BackLit backLit;
  #endif

#endif
