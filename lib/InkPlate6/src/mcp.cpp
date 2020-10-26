/*
Mcp.cpp
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

#include "mcp.h"
#include "wire.h"

// LOW LEVEL:

bool MCP::begin(uint8_t *_r)
{
  Wire::begin_transmission(address);
  int error = Wire::end_transmission();
  if (error) return false;
    
  read_registers(address, _r);
  _r[0] = 0xff;
  _r[1] = 0xff;
  update_all_register(address, _r);

  return true;
}

void MCP::read_registers(uint8_t * k)
{
  Wire::begin_transmission(address);
  Wire::write(0x00);
  Wire::end_transmission();
  Wire::request_from(address, (uint8_t) 22);
  for (int i = 0; i < 22; ++i) {
    k[i] = Wire::read();
  }
}

void MCP::read_registers(uint8_t _regName, uint8_t * k, uint8_t _n)
{
  Wire::begin_transmission(address);
  Wire::write(_regName);
  Wire::end_transmission();

  Wire::request_from(address, _n);
  for (int i = 0; i < _n; ++i) {
    k[_regName + i] = Wire::read();
  }
}

void MCP::read_register(uint8_t _regName, uint8_t * k)
{
  Wire::begin_transmission(address);
  Wire::write(_regName);
  Wire::end_transmission();
  Wire::request_from(address, (uint8_t) 1);
  k[_regName] = Wire::read();
}

void MCP::update_all_register(uint8_t * k)
{
  Wire::begin_transmission(address);
  Wire::write(0x00);
  for (int i = 0; i < 22; ++i) {
    Wire::write(k[i]);
  }
  Wire::end_transmission();
}

void MCP::update_register(uint8_t _regName, uint8_t _d)
{
  Wire::begin_transmission(address);
  Wire::write(_regName);
  Wire::write(_d);
  Wire::end_transmission();
}

void MCP::update_register(uint8_t _regName, uint8_t * k, uint8_t _n)
{
  Wire::begin_transmission(address);
  Wire::write(_regName);
  for (int i = 0; i < _n; ++i) {
    Wire::write(k[_regName + i]);
  }
  Wire::end_transmission();
}

// HIGH LEVEL:

void MCP::pin_mode(uint8_t _pin, PinMode _mode)
{
  uint8_t _port = (_pin >> 3) & 1;
  uint8_t _p = _pin & 0x07;

  switch (_mode) {
    case INPUT:
      mcpRegsInt[MCP23017_IODIRA + _port] |= 1 << _p;   // Set it to input
      mcpRegsInt[MCP23017_GPPUA + _port] &= ~(1 << _p); // Disable pullup on that pin
      update_register(MCP23017_IODIRA + _port, mcpRegsInt[MCP23017_IODIRA + _port]);
      update_register(MCP23017_GPPUA + _port, mcpRegsInt[MCP23017_GPPUA + _port]);
      break;

  case INPUT_PULLUP:
      mcpRegsInt[MCP23017_IODIRA + _port] |= 1 << _p; // Set it to input
      mcpRegsInt[MCP23017_GPPUA + _port] |= 1 << _p;  // Enable pullup on that pin
      update_register(MCP23017_IODIRA + _port, mcpRegsInt[MCP23017_IODIRA + _port]);
      update_register(MCP23017_GPPUA + _port, mcpRegsInt[MCP23017_GPPUA + _port]);
      break;

  case OUTPUT:
      mcpRegsInt[MCP23017_IODIRA + _port] &= ~(1 << _p); // Set it to output
      mcpRegsInt[MCP23017_GPPUA + _port] &= ~(1 << _p);  // Disable pullup on that pin
      update_register(MCP23017_IODIRA + _port, mcpRegsInt[MCP23017_IODIRA + _port]);
      update_register(MCP23017_GPPUA + _port, mcpRegsInt[MCP23017_GPPUA + _port]);
      break;
  }
}

void MCP::digital_write(uint8_t _pin, uint8_t _state)
{
  uint8_t _port = (_pin / 8) & 1;
  uint8_t _p = _pin % 8;

  if (mcpRegsInt[MCP23017_IODIRA + _port] & (1 << _p)) return;
  _state ? (mcpRegsInt[MCP23017_GPIOA + _port] |= (1 << _p)) : (mcpRegsInt[MCP23017_GPIOA + _port] &= ~(1 << _p));
  update_register(MCP23017_GPIOA + _port, mcpRegsInt[MCP23017_GPIOA + _port]);
}

uint8_t MCP::digital_read(uint8_t _pin)
{
  uint8_t _port = (_pin / 8) & 1;
  uint8_t _p = _pin % 8;
  readMCPRegister(MCP23017_GPIOA + _port, mcpRegsInt);
  return (mcpRegsInt[MCP23017_GPIOA + _port] & (1 << _p)) ? HIGH : LOW;
}

void MCP::setIntOutput(uint8_t intPort, uint8_t mirroring, uint8_t openDrain, uint8_t polarity)
{
  intPort   &= 1;
  mirroring &= 1;
  openDrain &= 1;
  polarity  &= 1;
  mcpRegsInt[MCP23017_IOCONA + intPort] = (mcpRegsInt[MCP23017_IOCONA + intPort] & ~(1 << 6)) | (1 << mirroring);
  mcpRegsInt[MCP23017_IOCONA + intPort] = (mcpRegsInt[MCP23017_IOCONA + intPort] & ~(1 << 6)) | (1 << openDrain);
  mcpRegsInt[MCP23017_IOCONA + intPort] = (mcpRegsInt[MCP23017_IOCONA + intPort] & ~(1 << 6)) | (1 << polarity);
  update_register(MCP23017_IOCONA + intPort, mcpRegsInt[MCP23017_IOCONA + intPort]);
}

void MCP::setIntPin(uint8_t _pin, IntMode _mode)
{
  uint8_t _port = (_pin / 8) & 1;
  uint8_t _p = _pin % 8;

  switch (_mode) {
    case CHANGE:
      mcpRegsInt[MCP23017_INTCONA + _port] &= ~(1 << _p);
      break;

    case FALLING:
      mcpRegsInt[MCP23017_INTCONA + _port] |= (1 << _p);
      mcpRegsInt[MCP23017_DEFVALA + _port] |= (1 << _p);
      break;

    case RISING:
      mcpRegsInt[MCP23017_INTCONA + _port] |= (1 << _p);
      mcpRegsInt[MCP23017_DEFVALA + _port] &= ~(1 << _p);
      break;
  }
  mcpRegsInt[MCP23017_GPINTENA + _port] |= (1 << _p);
  update_register(MCP23017_GPINTENA, mcpRegsInt, 6);
}

void MCP::removeIntPin(uint8_t _pin)
{
  uint8_t _port = (_pin / 8) & 1;
  uint8_t _p = _pin % 8;
  mcpRegsInt[MCP23017_GPINTENA + _port] &= ~(1 << _p);
  update_register(MCP23017_GPINTENA, mcpRegsInt, 2);
}

uint16_t MCP::getINT()
{
  readMCPRegisters(MCP23017_INTFA, mcpRegsInt, 2);
  return ((mcpRegsInt[MCP23017_INTFB] << 8) | mcpRegsInt[MCP23017_INTFA]);
}

uint16_t MCP::getINTstate()
{
  readMCPRegisters(MCP23017_INTCAPA, mcpRegsInt, 2);
  return ((mcpRegsInt[MCP23017_INTCAPB] << 8) | mcpRegsInt[MCP23017_INTCAPA]);
}

void MCP::setPorts(uint16_t _d)
{
  mcpRegsInt[MCP23017_GPIOA] = _d & 0xff;
  mcpRegsInt[MCP23017_GPIOB] = (_d >> 8) & 0xff;
  update_register(MCP23017_GPIOA, mcpRegsInt, 2);
}

uint16_t MCP::getPorts()
{
  readMCPRegisters(MCP23017_GPIOA, mcpRegsInt, 2);
  return ((mcpRegsInt[MCP23017_GPIOB] << 8) | (mcpRegsInt[MCP23017_GPIOA]));
}