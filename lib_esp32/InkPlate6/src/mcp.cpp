/*
Mcp.cpp
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

#define __MCP__ 1
#include "mcp.hpp"

#include "logging.hpp"

#include "wire.hpp"
#include "driver/gpio.h"

MCP MCP::singleton;

// LOW LEVEL:

void MCP::test()
{
  printf("Registers before read:\n");
  for (int i = 0; i < 22; i++) {
    printf("%02x ", registers[i]);
  }
  printf("\n");
  fflush(stdout);

  wire.begin_transmission(MCP_ADDRESS);
  wire.end_transmission();

  read_all_registers();

  printf("Registers after read:\n");
  for (int i = 0; i < 22; i++) {
    printf("%02x ", registers[i]);
  }
  printf("\n");
  fflush(stdout);
}

bool MCP::setup()
{
  wire.begin_transmission(MCP_ADDRESS);
  wire.end_transmission();
    
  read_all_registers();
  registers[0] = 0xff;
  registers[1] = 0xff;
  update_all_registers();

  return true;
}

void MCP::read_all_registers()
{
  wire.begin_transmission(MCP_ADDRESS);
  wire.write(0x00);
  wire.end_transmission();
  wire.request_from(MCP_ADDRESS, (uint8_t) 22);
  for (int i = 0; i < 22; ++i) {
    registers[i] = wire.read();
  }
}

void MCP::read_registers(Reg first_reg, uint8_t count)
{
  wire.begin_transmission(MCP_ADDRESS);
  wire.write((int8_t)first_reg);
  wire.end_transmission();

  wire.request_from(MCP_ADDRESS, count);
  for (int i = 0; i < count; ++i) {
    registers[(int8_t)first_reg + i] = wire.read();
  }
}

uint8_t MCP::read_register(Reg reg)
{
  wire.begin_transmission(MCP_ADDRESS);
  wire.write((int8_t)reg);
  wire.end_transmission();
  wire.request_from(MCP_ADDRESS, (uint8_t) 1);
  registers[(int8_t)reg] = wire.read();

  return registers[(int8_t)reg];
}

void MCP::update_all_registers()
{
  wire.begin_transmission(MCP_ADDRESS);
  wire.write(0x00);
  for (int i = 0; i < 22; ++i) {
    wire.write(registers[i]);
  }
  wire.end_transmission();
}

void MCP::update_register(Reg reg, uint8_t value)
{
  wire.begin_transmission(MCP_ADDRESS);
  wire.write((int8_t)reg);
  wire.write(value);
  wire.end_transmission();
}

void MCP::update_registers(Reg first_reg, uint8_t count)
{
  wire.begin_transmission(MCP_ADDRESS);
  wire.write((int8_t)first_reg);
  for (int i = 0; i < count; ++i) {
    wire.write(registers[(int8_t)first_reg + i]);
  }
  wire.end_transmission();
}

// HIGH LEVEL:

void MCP::set_direction(Pin pin, PinMode mode)
{
  uint8_t port = ((uint8_t)pin >> 3) & 1;
  uint8_t p    =  (uint8_t)pin & 7;

  switch (mode) {
    case PinMode::INPUT:
      registers[(int8_t)Reg::IODIRA + port] |=   1 << p;  // Set it to input
      registers[(int8_t)Reg::GPPUA  + port] &= ~(1 << p); // Disable pullup on that pin
      update_register((Reg)((int8_t)Reg::IODIRA + port), registers[(int8_t)Reg::IODIRA + port]);
      update_register((Reg)((int8_t)Reg::GPPUA  + port), registers[(int8_t)Reg::GPPUA  + port]);
      break;

  case PinMode::INPUT_PULLUP:
      registers[(int8_t)Reg::IODIRA + port] |= 1 << p; // Set it to input
      registers[(int8_t)Reg::GPPUA  + port] |= 1 << p; // Enable pullup on that pin
      update_register((Reg)((int8_t)Reg::IODIRA + port), registers[(int8_t)Reg::IODIRA + port]);
      update_register((Reg)((int8_t)Reg::GPPUA  + port), registers[(int8_t)Reg::GPPUA  + port]);
      break;

  case PinMode::OUTPUT:
      registers[(int8_t)Reg::IODIRA + port] &= ~(1 << p); // Set it to output
      registers[(int8_t)Reg::GPPUA  + port] &= ~(1 << p); // Disable pullup on that pin
      update_register((Reg)((int8_t)Reg::IODIRA + port), registers[(int8_t)Reg::IODIRA + port]);
      update_register((Reg)((int8_t)Reg::GPPUA  + port), registers[(int8_t)Reg::GPPUA  + port]);
      break;
  }
}

void MCP::digital_write(Pin pin, uint8_t state)
{
  uint8_t port = ((uint8_t)pin >> 3) & 1;
  uint8_t p    =  (uint8_t)pin & 7;

  if (registers[(int8_t)Reg::IODIRA + port] & (1 << p)) return;
  state ? (registers[(int8_t)Reg::GPIOA + port] |= (1 << p)) : (registers[(int8_t)Reg::GPIOA + port] &= ~(1 << p));
  update_register((Reg)((int8_t)Reg::GPIOA + port), registers[(int8_t)Reg::GPIOA + port]);
}

uint8_t MCP::digital_read(Pin pin)
{
  uint8_t port = ((uint8_t)pin >> 3) & 1;
  uint8_t p    =  (uint8_t)pin & 7;
  uint8_t r = read_register((Reg)((int8_t)Reg::GPIOA + port));

  return (r & (1 << p)) ? HIGH : LOW;
}

void MCP::set_int_output(IntPort intPort, bool mirroring, bool openDrain, uint8_t polarity)
{
  uint8_t reg = (uint8_t)Reg::IOCONA + (uint8_t)intPort;

  polarity  &= 1;

  registers[reg] = (registers[reg] & ~(1 << 6)) | (mirroring << 6);
  registers[reg] = (registers[reg] & ~(1 << 2)) | (openDrain << 2);
  registers[reg] = (registers[reg] & ~(1 << 1)) | (polarity  << 1);
  
  update_register((Reg)(reg), registers[reg]);
}

void MCP::set_int_pin(Pin pin, IntMode mode)
{
  uint8_t port = ((uint8_t)pin >> 3) & 1;
  uint8_t p    =  (uint8_t)pin & 7;

  switch (mode) {
    case IntMode::CHANGE:
      registers[(int8_t)Reg::INTCONA + port] &= ~(1 << p);
      break;

    case IntMode::FALLING:
      registers[(int8_t)Reg::INTCONA + port] |= (1 << p);
      registers[(int8_t)Reg::DEFVALA + port] |= (1 << p);
      break;

    case IntMode::RISING:
      registers[(int8_t)Reg::INTCONA + port] |=  (1 << p);
      registers[(int8_t)Reg::DEFVALA + port] &= ~(1 << p);
      break;
  }
  registers[(int8_t)Reg::GPINTENA + port] |= (1 << p);
  update_registers(Reg::GPINTENA, 6);
}

void MCP::remove_int_pin(Pin pin)
{
  uint8_t port = ((uint8_t)pin >> 3) & 1;
  uint8_t p    =  (uint8_t)pin & 7;
  registers[(int8_t)Reg::GPINTENA + port] &= ~(1 << p);
  update_registers(Reg::GPINTENA, 2);
}

uint16_t MCP::get_int()
{
  read_registers(Reg::INTFA, 2);
  return ((registers[(int8_t)Reg::INTFB] << 8) | registers[(int8_t)Reg::INTFA]);
}

uint16_t MCP::get_int_state()
{
  read_registers(Reg::INTCAPA, 2);
  return ((registers[(int8_t)Reg::INTCAPB] << 8) | registers[(int8_t)Reg::INTCAPA]);
}

void MCP::set_ports(uint16_t values)
{
  registers[(int8_t)Reg::GPIOA] = values & 0xff;
  registers[(int8_t)Reg::GPIOB] = (values >> 8) & 0xff;
  update_registers(Reg::GPIOA, 2);
}

uint16_t MCP::get_ports()
{
  read_registers(Reg::GPIOA, 2);
  return ((registers[(int8_t)Reg::GPIOB] << 8) | (registers[(int8_t)Reg::GPIOA]));
}