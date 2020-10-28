/*
InkPlate6.h
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

#ifndef __INKPLATE6_HPP__
#define __INKPLATE6_HPP__

#include "defines.hpp"

class InkPlate6
{
  public:
    InkPlate6();

    uint8_t read_touchpad(uint8_t _pad);
    double  read_battery();
};

#if INKPLATE6
  InkPlate6 ink_plate_6;
#else
  extern InkPlate6 ink_plate_6;
#endif

#endif