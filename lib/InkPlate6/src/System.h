/*
System.h
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

#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include "../libs/SdFat/SdFat.h"
#include "Arduino.h"
#include "SPI.h"

#include "defines.h"
#include "Esp.h"
#include "Mcp.h"
#include "NetworkClient.h"

class System : public Mcp
{
  public:
    void setPanelState(uint8_t s);
    uint8_t getPanelState();

    void setSdCardOk(int16_t s);
    int16_t getSdCardOk();

    int8_t readTemperature();

    uint8_t readTouchpad(uint8_t _pad);
    double readBattery();

    int16_t sdCardInit();

    SdFat getSdFat();
    SPIClass getSPI();

  private:
    uint8_t _panelOn = 0;
    int16_t _sdCardOk = 0;
};

#endif