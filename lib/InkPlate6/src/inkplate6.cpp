/*
InkPlate6.cpp
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

#include "inkplate6.hpp"

#include "wire.hpp"
#include "mcp.hpp"
#include "esp.hpp"
#include "eink.hpp"

int8_t 
InkPlate6::read_temperature()
{
  int8_t temp;
    
  if (e_ink.get_panel_state() == EInk::OFF) {
    mcp.WAKEUP_SET();
    mcp.PWRUP_SET();
    ESP::delay(5);
  }

  wire.begin_transmission(0x48);
  wire.write(0x0D);
  wire.write(0b10000000);
  wire.end_transmission();
    
  ESP::delay(5);

  wire.begin_transmission(0x48);
  wire.write(0x00);
  wire.end_transmission();

  wire.request_from(0x48, 1);
  temp = wire.read();
    
  if (e_ink.get_panel_state() == EInk::OFF) {
    mcp.PWRUP_CLEAR();
    mcp.WAKEUP_CLEAR();
    ESP::delay(5);
  }

  return temp;
}

uint8_t 
InkPlate6::read_touchpad(uint8_t pad)
{
  return mcp.digital_read((MCP::Pin)((pad & 3) + 10));
}

double 
InkPlate6::read_battery()
{
  mcp.digital_write(MCP::BATTERY_SWITCH, HIGH);
  ESP::delay(1);
  int16_t adc = ESP::analog_read(GPIO_NUM_35);
  mcp.digital_write(MCP::BATTERY_SWITCH, LOW);

  return ((double(adc) / 4095.0) * 3.3 * 2);
}
