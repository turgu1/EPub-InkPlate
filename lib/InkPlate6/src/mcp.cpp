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

#define __MCP__ 1
#include "mcp.hpp"

#include "wire.hpp"

#include "driver/gpio.h"

MCP MCP::singleton;

// LOW LEVEL:

bool MCP::begin()
{
  wire.begin_transmission(address);
  int error = wire.end_transmission();
  if (error) return false;
    
  read_registers(registers);
  registers[0] = 0xff;
  registers[1] = 0xff;
  update_all_register(registers);

  return true;
}

void MCP::read_registers(uint8_t * k)
{
  wire.begin_transmission(address);
  wire.write(0x00);
  wire.end_transmission();
  wire.request_from(address, (uint8_t) 22);
  for (int i = 0; i < 22; ++i) {
    k[i] = wire.read();
  }
}

void MCP::read_registers(Reg reg, uint8_t * k, uint8_t count)
{
  wire.begin_transmission(address);
  wire.write(reg);
  wire.end_transmission();

  wire.request_from(address, count);
  for (int i = 0; i < count; ++i) {
    k[reg + i] = wire.read();
  }
}

void MCP::read_register(Reg reg, uint8_t * k)
{
  wire.begin_transmission(address);
  wire.write(reg);
  wire.end_transmission();
  wire.request_from(address, (uint8_t) 1);
  k[reg] = wire.read();
}

void MCP::update_all_register(uint8_t * k)
{
  wire.begin_transmission(address);
  wire.write(0x00);
  for (int i = 0; i < 22; ++i) {
    wire.write(k[i]);
  }
  wire.end_transmission();
}

void MCP::update_register(Reg reg, uint8_t _d)
{
  wire.begin_transmission(address);
  wire.write(reg);
  wire.write(_d);
  wire.end_transmission();
}

void MCP::update_register(Reg reg, uint8_t * k, uint8_t _n)
{
  wire.begin_transmission(address);
  wire.write(reg);
  for (int i = 0; i < _n; ++i) {
    wire.write(k[reg + i]);
  }
  wire.end_transmission();
}

// HIGH LEVEL:

void MCP::set_direction(Pin pin, PinMode mode)
{
  uint8_t port = (pin >> 3) & 1;
  uint8_t p    =  pin &  7;

  switch (mode) {
    case INPUT:
      registers[IODIRA + port] |=   1 << p;    // Set it to input
      registers[GPPUA  + port] &= ~(1 << p); // Disable pullup on that pin
      update_register((Reg)(IODIRA + port), registers[IODIRA + port]);
      update_register((Reg)(GPPUA  + port), registers[GPPUA  + port]);
      break;

  case INPUT_PULLUP:
      registers[IODIRA + port] |= 1 << p; // Set it to input
      registers[GPPUA  + port] |= 1 << p; // Enable pullup on that pin
      update_register((Reg)(IODIRA + port), registers[IODIRA + port]);
      update_register((Reg)(GPPUA  + port), registers[GPPUA  + port]);
      break;

  case OUTPUT:
      registers[IODIRA + port] &= ~(1 << p); // Set it to output
      registers[GPPUA  + port] &= ~(1 << p); // Disable pullup on that pin
      update_register((Reg)(IODIRA + port), registers[IODIRA + port]);
      update_register((Reg)(GPPUA  + port), registers[GPPUA  + port]);
      break;
  }
}

void MCP::digital_write(Pin pin, uint8_t state)
{
  uint8_t port = (pin / 8) & 1;
  uint8_t _p = pin % 8;

  if (registers[IODIRA + port] & (1 << _p)) return;
  state ? (registers[GPIOA + port] |= (1 << _p)) : (registers[GPIOA + port] &= ~(1 << _p));
  update_register((Reg)(GPIOA + port), registers[GPIOA + port]);
}

uint8_t MCP::digital_read(Pin pin)
{
  uint8_t port = (pin >> 3) & 1;
  uint8_t p    =  pin & 7;
  read_register((Reg)(GPIOA + port), registers);
  return (registers[GPIOA + port] & (1 << p)) ? HIGH : LOW;
}

void MCP::set_int_output(uint8_t intPort, uint8_t mirroring, uint8_t openDrain, uint8_t polarity)
{
  intPort   &= 1;
  mirroring &= 1;
  openDrain &= 1;
  polarity  &= 1;

  registers[IOCONA + intPort] = (registers[IOCONA + intPort] & ~(1 << 6)) | (1 << mirroring);
  registers[IOCONA + intPort] = (registers[IOCONA + intPort] & ~(1 << 6)) | (1 << openDrain);
  registers[IOCONA + intPort] = (registers[IOCONA + intPort] & ~(1 << 6)) | (1 << polarity);
  update_register((Reg)(IOCONA + intPort), registers[IOCONA + intPort]);
}

void MCP::set_int_pin(Pin pin, IntMode mode)
{
  uint8_t port = (pin / 8) & 1;
  uint8_t p = pin % 8;

  switch (mode) {
    case CHANGE:
      registers[INTCONA + port] &= ~(1 << p);
      break;

    case FALLING:
      registers[INTCONA + port] |= (1 << p);
      registers[DEFVALA + port] |= (1 << p);
      break;

    case RISING:
      registers[INTCONA + port] |=  (1 << p);
      registers[DEFVALA + port] &= ~(1 << p);
      break;
  }
  registers[GPINTENA + port] |= (1 << p);
  update_register(GPINTENA, registers, 6);
}

void MCP::remove_int_pin(Pin pin)
{
  uint8_t port = (pin / 8) & 1;
  uint8_t p = pin % 8;
  registers[GPINTENA + port] &= ~(1 << p);
  update_register(GPINTENA, registers, 2);
}

uint16_t MCP::get_int()
{
  read_registers(INTFA, registers, 2);
  return ((registers[INTFB] << 8) | registers[INTFA]);
}

uint16_t MCP::get_int_state()
{
  read_registers(INTCAPA, registers, 2);
  return ((registers[INTCAPB] << 8) | registers[INTCAPA]);
}

void MCP::set_ports(uint16_t d)
{
  registers[GPIOA] = d & 0xff;
  registers[GPIOB] = (d >> 8) & 0xff;
  update_register(GPIOA, registers, 2);
}

uint16_t MCP::get_ports()
{
  read_registers(GPIOA, registers, 2);
  return ((registers[GPIOB] << 8) | (registers[GPIOA]));
}