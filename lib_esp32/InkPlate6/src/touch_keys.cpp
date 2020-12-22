#define __TOUCH_KEYS__ 1
#include "touch_keys.hpp"
#include "wire.hpp"

TouchKeys TouchKeys::singleton;

TouchKeys::TouchKeys() {}

bool
TouchKeys::setup() 
{
  Wire::enter();
  mcp.set_direction(TOUCH_0, MCP::PinMode::INPUT);
  mcp.set_direction(TOUCH_1, MCP::PinMode::INPUT);
  mcp.set_direction(TOUCH_2, MCP::PinMode::INPUT);

  // Prepare the MCP device to allow for interrupts
  // coming from any of the touchkeys. Interrupts will be raised
  // for any change of state of the 3 touchkeys. The GPIO_NUM_34
  // must be programmed as per the ESP-IDF documentation to get
  // some interrupts.

  mcp.set_int_pin(TOUCH_0, MCP::IntMode::RISING);
  mcp.set_int_pin(TOUCH_1, MCP::IntMode::RISING);
  mcp.set_int_pin(TOUCH_2, MCP::IntMode::RISING);

  mcp.set_int_output(MCP::IntPort::INTPORTB, false, false, MCP::SignalLevel::HIGH);
  Wire::leave();  

  return true;
}

uint8_t 
TouchKeys::read_all_keys()
{
  Wire::enter();
  uint16_t value = mcp.get_ports();
  Wire::leave();
  return (value >> 10) & 7;  // Not clean
}

uint8_t 
TouchKeys::read_key(Key key)
{
  Wire::enter();
  MCP::SignalLevel value = mcp.digital_read((MCP::Pin)(((uint8_t)key & 3) + 10)); // Not clean
  Wire::leave();

  return value == MCP::SignalLevel::HIGH ? 1 : 0;
}
