/*
Mcp.h
Inkplate 6 Arduino library
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

#ifndef __MCP_H__
#define __MCP_H__

#define MCP23017_IODIRA 0x00
#define MCP23017_GPPUA  0x0C
#define MCP23017_ADDR   0x20

#define MCP23017_GPIOA    0x12
#define MCP23017_IOCONA   0x0A
#define MCP23017_INTCONA  0x08
#define MCP23017_GPINTENA 0x04
#define MCP23017_DEFVALA  0x06
#define MCP23017_INTFA    0x0E
#define MCP23017_INTCAPA  0x10
#define MCP23017_INTFB    0x0F
#define MCP23017_INTCAPB  0x11
#define MCP23017_GPIOB    0x13

#define OE_SET       mcp.digitalWrite(MCP::OE, HIGH);
#define OE_CLEAR     mcp.digitalWrite(MCP::OE, LOW);

#define GMOD_SET     mcp.digitalWrite(MCP::GMOD, HIGH);
#define GMOD_CLEAR   mcp.digitalWrite(MCP::GMOD, LOW);

#define SPV_SET      mcp.digitalWrite(MCP::SPV, HIGH);
#define SPV_CLEAR    mcp.digitalWrite(MCP::SPV, LOW);

#define WAKEUP_SET   mcp.digitalWrite(MCP::WAKEUP, HIGH);
#define WAKEUP_CLEAR mcp.digitalWrite(MCP::WAKEUP, LOW);

#define PWRUP_SET    mcp.digitalWrite(MCP::PWRUP, HIGH);
#define PWRUP_CLEAR  mcp.digitalWrite(MCP::PWRUP, LOW);

#define VCOM_SET     mcp.digitalWrite(MCP::VCOM, HIGH);
#define VCOM_CLEAR   mcp.digitalWrite(MCP::VCOM, LOW);

class Mcp
{
  private:
    static const uint8_t address = MCP23017_ADDR;
    uint8_t mcpRegsInt[22];

  public:
    enum PinMode : uint8_t { INPUT, INPUT_PULLUP, OUTPUT };
    enum IntMode : uint8_t { CHANGE, FALLING, RISING };
    enum Pin : uint8_t {
      OE     = 0,
      GMOD   = 1,
      SPV    = 2,
      WAKEUP = 3,
      PWRUP  = 4,
      VCOM   = 5,
      GPIO0_ENABLE = 8,
      BATTERY_SWITCH = 9,
      TOUCH_0 = 10,
      TOUCH_1 = 11,
      TOUCH_2 = 12
    };

    bool begin(uint8_t *_r);

    void readRegisters(uint8_t *k);
    void readRegisters(uint8_t _regName, uint8_t *k, uint8_t _n);
    void  readRegister(uint8_t _regName, uint8_t *k);
    
    void updateAllRegisters(uint8_t *k);
    
    void  updateRegister(uint8_t _regName, uint8_t _d);
    void  updateRegister(uint8_t _regName, uint8_t *k, uint8_t _n);

    void         pinMode(Pin _pin, PinMode _mode);
    void    digitalWrite(Pin _pin, uint8_t _state);
    uint8_t  digitalRead(Pin _pin);

    void setIntOutput(uint8_t intPort, uint8_t mirroring, uint8_t openDrain, uint8_t polarity);
    void    setIntPin(Pin _pin, uint8_t _mode);
    void removeIntPin(Pin _pin);

    uint16_t      getINT();
    uint16_t getINTstate();

    void     setPorts(uint16_t _d);
    uint16_t getPorts();
};

#if MCP
  Mcp mcp;
#else
  extern Mcp mcp;
#endif

#endif