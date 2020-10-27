#ifndef __WIRE_HPP__
#define __WIRE_HPP__

#include "defines.hpp"
#include "noncopyable.hpp"

#include "driver/i2c.h"

class Wire : NonCopyable
{
  private:
    bool initialized;
    static Wire singleton;
    Wire() : initialized(false) {}

  public:
    static inline Wire & get_singleton() noexcept { return singleton; }

    esp_err_t begin();
    void      begin_transmission(uint8_t addr);
    int       end_transmission();
    void      write(uint8_t val);
    uint8_t   read();
    void      request_from(uint8_t addr, uint8_t size);
};

#if __WIRE__
  Wire & wire = Wire::get_singleton();
#else
  extern Wire & wire;
#endif

#endif