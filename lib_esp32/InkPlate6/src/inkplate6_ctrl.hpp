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
  static constexpr char const * TAG = "InkPlate6Ctrl";

  static InkPlate6Ctrl singleton;
  InkPlate6Ctrl() {};

  public:
    static inline InkPlate6Ctrl & get_singleton() noexcept { return singleton; }

    /**
     * @brief Setup the InkPlate-6 Controller
     * 
     * This method initialize the SD-Card access, the e-Ink display and the touchpad interrupt
     * capability.
     * 
     * @return true 
     */
    bool setup();

    /**
     * @brief SD-Card Setup
     * 
     * This method initialize the ESP-IDF SD-Card capability. This will allow access
     * to the card through standard Posix IO functions or the C++ IOStream.
     * 
     * @return true Initialization Done
     * @return false Some issue
     */
    bool sd_card_setup();

    /**
     * @brief Touchpad Interrupt Capability setup
     * 
     * This method prepare the MCP device to allow for interrupts
     * coming from any of the touchpad. Interrupts will be raised
     * for any change of state of the 3 touchpads. The GPIO_NUM_34
     * must be programmed as per the ESP-IDF documentation to get
     * some interrupts.
     * 
     * @return true 
     * @return false 
     */
    bool touchpad_int_setup();

    /**
     * @brief Read a specific touchpad
     * 
     * Read one of the three touchpad.
     * 
     * @param pad touchpad number (0, 1 or 2)
     * @return uint8_t 1 if touched, 0 if not
     */
    uint8_t read_touchpad(uint8_t pad);

    /**
     * @brief Read all touchpads
     * 
     * @return uint8_t touchpad bitmap (bit = 1 if touched) touchpads 0, 1 and 2 are mapped to bits 0, 1 and 2.
     */
    uint8_t read_touchpads();

    double  read_battery();

    bool light_sleep(uint32_t minutes_to_sleep);
    void deep_sleep();
};

#if __INKPLATE6_CTRL__
  InkPlate6Ctrl & inkplate6_ctrl = InkPlate6Ctrl::get_singleton();
#else
  extern InkPlate6Ctrl & inkplate6_ctrl;
#endif

#endif