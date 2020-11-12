/*
MCP.h
Inkplate 6 ESP-IDF

Modified by Guy Turcotte 
November 12, 2020

from the Arduino Library:

David Zovko, Borna Biro, Denis Vajak, Zvonimir Haramustek @ e-radionica.com
September 24, 2020
https://github.com/e-radionicacom/Inkplate-6-Arduino-library

For support, please reach over forums: forum.e-radionica.com/en
For more info about the product, please check: www.inkplate.io

This code is released under the GNU Lesser General Public License v3.0: https://www.gnu.org/licenses/lgpl-3.0.en.html
Please review the LICENSE file included with this example.
If you have any questions about licensing, please contact techsupport@e-radionica.com
Distributed as-is; no warranty is given.
*/

#ifndef __MCP_HPP__
#define __MCP_HPP__

#include <cinttypes>
#include <cstring>

#include "noncopyable.hpp"
#include "esp.hpp"

class MCP : NonCopyable
{
  private:
    static constexpr char const * TAG = "MCP";

    static const uint8_t MCP_ADDRESS = 0x20;
    uint8_t registers[22];

    static MCP singleton;
    MCP() { memset(registers, 0, 22); }; // Not instanciable

  public:
    static inline MCP & get_singleton() noexcept { return singleton; }

    enum PinMode : uint8_t { INPUT,  INPUT_PULLUP, OUTPUT };
    enum IntMode : uint8_t { CHANGE, FALLING,      RISING };
    enum Reg     : uint8_t {
      IODIRA   = 0x00,
      GPINTENA = 0x04,
      DEFVALA  = 0x06,
      INTCONA  = 0x08,
      IOCONA   = 0x0A,
      GPPUA    = 0x0C,
      INTFA    = 0x0E,
      INTFB    = 0x0F,
      INTCAPA  = 0x10,
      INTCAPB  = 0x11,
      GPIOA    = 0x12,
      GPIOB    = 0x13,  
    };

    enum Pin : uint8_t {
      OE             =  0,
      GMOD           =  1,
      SPV            =  2,
      WAKEUP         =  3,
      PWRUP          =  4,
      VCOM           =  5,
      GPIO0_ENABLE   =  8,
      BATTERY_SWITCH =  9,
      TOUCH_0        = 10,
      TOUCH_1        = 11,
      TOUCH_2        = 12
    };

    void test();
    
    bool setup();

    void read_registers(uint8_t * k);
    void read_registers(Reg reg, uint8_t * k, uint8_t n);
    void  read_register(Reg reg, uint8_t * k);
    
    void update_all_register(uint8_t * k);
    
    void  update_register(Reg reg, uint8_t   d);
    void  update_register(Reg reg, uint8_t * k, uint8_t n);

    void    set_direction(Pin pin, PinMode mode);
    void    digital_write(Pin pin, uint8_t state);
    uint8_t  digital_read(Pin pin);

    void set_int_output(uint8_t intPort, uint8_t mirroring, uint8_t openDrain, uint8_t polarity);
    void    set_int_pin(Pin pin, IntMode mode);
    void remove_int_pin(Pin pin);

    uint16_t      get_int();
    uint16_t get_int_state();

    void     set_ports(uint16_t d);
    uint16_t get_ports();

    inline void oe_set()       { digital_write(OE,     HIGH); }
    inline void oe_clear()     { digital_write(OE,     LOW ); }

    inline void gmod_set()     { digital_write(GMOD,   HIGH); }
    inline void gmod_clear()   { digital_write(GMOD,   LOW ); }

    inline void spv_set()      { digital_write(SPV,    HIGH); }
    inline void spv_clear()    { digital_write(SPV,    LOW ); }

    inline void wakeup_set()   { digital_write(WAKEUP, HIGH); }
    inline void wakeup_clear() { digital_write(WAKEUP, LOW ); }

    inline void pwrup_set()    { digital_write(PWRUP,  HIGH); }
    inline void pwrup_clear()  { digital_write(PWRUP,  LOW ); }

    inline void vcom_set()     { digital_write(VCOM,   HIGH); }
    inline void vcom_clear()   { digital_write(VCOM,   LOW ); }
};

// Singleton

#if __MCP__
  MCP & mcp = MCP::get_singleton();
#else
  extern MCP & mcp;
#endif

#endif