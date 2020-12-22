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

#include <array>

// this is a new kind of array which accepts and requires its indices to be enums
template<typename E, class T, std::size_t N>
class enum_array : public std::array<T, N> {
public:
    T & operator[] (E e) {
        return std::array<T, N>::operator[]((std::size_t)e);
    }

    const T & operator[] (E e) const {
        return std::array<T, N>::operator[]((std::size_t)e);
    }
};

class MCP : NonCopyable
{
  private:
    static constexpr char const * TAG = "MCP";
    enum class Reg : uint8_t {
      IODIRA   = 0x00,
      IODIRB   = 0x01,
      IPOLA    = 0x02,
      IPOLB    = 0x03,
      GPINTENA = 0x04,
      GPINTENB = 0x05,
      DEFVALA  = 0x06,
      DEFVALB  = 0x07,
      INTCONA  = 0x08,
      INTCONB  = 0x09,
      IOCONA   = 0x0A,
      IOCONB   = 0x0B,
      GPPUA    = 0x0C,
      GPPUB    = 0x0D,
      INTFA    = 0x0E,
      INTFB    = 0x0F,
      INTCAPA  = 0x10,
      INTCAPB  = 0x11,
      GPIOA    = 0x12,
      GPIOB    = 0x13, 
      OLATA    = 0x14,
      OLATB    = 0x15 
    };

    static const uint8_t MCP_ADDRESS = 0x20;
    enum_array<Reg, uint8_t, 22> registers;
 
    // Adjust Register, adding offset p
    inline Reg R(Reg r, uint8_t p) { return (Reg)((uint8_t)r + p); }

    static MCP singleton;
    MCP() { std::fill(registers.begin(), registers.end(), 0); }; // Not instanciable

    void read_all_registers();
    
    void   read_registers(Reg first_reg, uint8_t count);
    uint8_t read_register(Reg reg);
    
    void update_all_registers();
    
    void   update_register(Reg reg,       uint8_t  value);
    void  update_registers(Reg first_reg, uint8_t  count);

  public:
    static inline MCP & get_singleton() noexcept { return singleton; }

    enum class PinMode : uint8_t { INPUT,    INPUT_PULLUP, OUTPUT };
    enum class IntMode : uint8_t { CHANGE,   FALLING,      RISING };
    enum class IntPort : uint8_t { INTPORTA, INTPORTB             };
    enum class SignalLevel { LOW, HIGH };

    enum class Pin : uint8_t {
      IOPIN_0,
      IOPIN_1,
      IOPIN_2,
      IOPIN_3,
      IOPIN_4,
      IOPIN_5,
      IOPIN_6,
      IOPIN_7,
      IOPIN_8,
      IOPIN_9,
      IOPIN_10,
      IOPIN_11,
      IOPIN_12,
      IOPIN_13,
      IOPIN_14,
      IOPIN_15
    };

    // BEFORE CALLING ANY OF THE FOLLOWING METHODS, ENSURE THAT THE Wire I2C
    // CLASS IS PROTECTED THROUGH THE USE OF Wire::enter() and Wire::leave() METHODS.
    
    void test();
    
    bool setup();

    void        set_direction(Pin pin, PinMode     mode );
    void        digital_write(Pin pin, SignalLevel state);
    SignalLevel  digital_read(Pin pin);

    void set_int_output(IntPort intPort, bool mirroring, bool openDrain, SignalLevel polarity);
    void    set_int_pin(Pin pin, IntMode mode);
    void remove_int_pin(Pin pin);

    uint16_t       get_int();
    uint16_t get_int_state();

    void     set_ports(uint16_t values);
    uint16_t get_ports();
};

// Singleton

#if __MCP__
  MCP & mcp = MCP::get_singleton();
#else
  extern MCP & mcp;
#endif

#endif