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

#include "inkplate6.h"

#include "wire.h"
#include "mcp.h"

void 
InkPlate6::set_panel_state(PanelState s)
{
  panel_state = s;
}

InkPlate6::PanelState 
InkPlate6::get_panel_state()
{
  return panel_state;
}

int8_t 
InkPlate6::read_temperature()
{
  int8_t temp;
    
  if (get_panel_state() == OFF) {
    mcp.WAKEUP_SET();
    mcp.PWRUP_SET();
    delay(5);
  }

  Wire::begin_transmission(0x48);
  Wire::write(0x0D);
  Wire::write(0b10000000);
  Wire::end_transmission();
    
  delay(5);

  Wire::begin_transmission(0x48);
  Wire::write(0x00);
  Wire::end_transmission();

  Wire::request_from(0x48, 1);
  temp = Wire::read();
    
  if (get_panel_state() == OFF) {
    mcp.PWRUP_CLEAR();
    mcp.WAKEUP_CLEAR();
    delay(5);
  }

  return temp;
}

uint8_t 
InkPlate6::read_touchpad(uint8_t _pad)
{
  return mcp.digital_read((_pad & 3) + 10);
}

double 
InkPlate6::read_battery()
{
  mcp.digital_write(MCP::BATTERY_SWITCH, HIGH);
  delay(1);
  int adc = analog_read(35);
  mcp.digital_write(MCP::BATTERY_SWITCH, LOW);

  return ((double(adc) / 4095.0) * 3.3 * 2);
}
