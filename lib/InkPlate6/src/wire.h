#ifndef __WIRE_H__
#define __WIRE_H__

#include "defines.h"

class Wire
{
  public:
    inline static void begin() { };
    inline static void begin_transmission(uint8_t addr) { };
    inline static void end_transmission() { };
    inline static void write(uint8_t val) { };
    inline static uint8_t read() { return 0; };
    inline static void request_from(uint8_t addr, uint8_t size) { };
};

// Integration with ESP-IDF:
//
// delay
// pin_mode
// INPUT, OUTPUT, INPUT_PULLUP
// ps_malloc, ps_free
// GPIO
// 

// ?? LUTB LUTW  LUT2

#endif