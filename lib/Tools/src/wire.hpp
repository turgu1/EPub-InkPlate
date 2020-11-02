#ifndef __WIRE_HPP__
#define __WIRE_HPP__

#include "defines.hpp"
#include "noncopyable.hpp"

#include "driver/i2c.h"

class Wire : NonCopyable
{
  private:
    static const uint8_t BUFFER_LENGTH = 30;
    
    bool    initialized;
    static  Wire singleton;

    uint8_t buffer[BUFFER_LENGTH];
    uint8_t address;
    uint8_t size_to_read;
    uint8_t index;

    Wire() : 
      initialized(false), 
      size_to_read(0),
      index(0)
      {}
    
    i2c_cmd_handle_t cmd;

  public:
    static inline Wire & get_singleton() noexcept { return singleton; }

    void       initialize();
    void       begin_transmission(uint8_t addr);
    void       end_transmission();
    void       write(uint8_t val);
    uint8_t    read();
    esp_err_t  request_from(uint8_t addr, uint8_t size);
};

#if __WIRE__
  Wire & wire = Wire::get_singleton();
#else
  extern Wire & wire;
#endif

#endif