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

#include <cstdint>

#include "noncopyable.hpp"

class InkPlate6Ctrl : NonCopyable
{
private:
  static InkPlate6Ctrl singleton;
  InkPlate6Ctrl() {};

  public:
    static inline InkPlate6Ctrl & get_singleton() noexcept { return singleton; }

    bool setup();
    bool sd_card_setup();

    uint8_t read_touchpad(uint8_t _pad);
    double  read_battery();
};

#if __INKPLATE6_CTRL__
  InkPlate6Ctrl & inkplate6_ctrl = InkPlate6Ctrl::get_singleton();
#else
  extern InkPlate6Ctrl & inkplate6_ctrl;
#endif

#endif