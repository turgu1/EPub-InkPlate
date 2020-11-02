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

#include "inkplate6_ctrl.hpp"

#include "logging.hpp"

#include "wire.hpp"
#include "mcp.hpp"
#include "esp.hpp"
#include "eink.hpp"

static const char * TAG = "InkPlate6Ctrl";

uint8_t 
InkPlate6Ctrl::read_touchpad(uint8_t pad)
{
  return mcp.digital_read((MCP::Pin)((pad & 3) + 10));
}

double 
InkPlate6Ctrl::read_battery()
{
  mcp.digital_write(MCP::BATTERY_SWITCH, HIGH);
  ESP::delay(1);
  int16_t adc = ESP::analog_read(ADC1_CHANNEL_7); // ADC 1 Channel 7 is GPIO port 35
  mcp.digital_write(MCP::BATTERY_SWITCH, LOW);

  ESP_LOGE(TAG, "Battery adc readout: %d.", adc);
  
  return ((double(adc) / 4095.0) * 3.3 * 2);
}
