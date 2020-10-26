/*
eink.h
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

#ifndef __EINK_H__
#define __EINK_H__

#include "defines.h"

/**
 * @brief Low level e-Ink display
 * 
 * This class implements the low level methods required to control
 * and access the e-ink display of the InkPlate-6 device.
 * 
 */

class EInk
{
  public:
    static const uint16_t WIDTH  = 800;
    static const uint16_t HEIGHT = 600;
    static const uint16_t BITMAP_SIZE_1BIT = (WIDTH * HEIGHT) >> 3;
    static const uint32_t BITMAP_SIZE_3BIT = ((uint32_t) WIDTH * HEIGHT) >> 1;

    enum Mode { B1, B3 };

  private:
    static const uint8_t EINK_ADDRESS = 0x48;


    static EInk * instance;

    bool initialized;
    Mode mode;

    void precalculateGamma(uint8_t * c, float gamma);

    void update_1b();
    void update_3b();

    void vscan_start();
    void vscan_end();
    void hscan_start(uint32_t _d = 0);
    
    void pins_z_state();
    void pins_as_outputs();

    uint32_t pin_LUT[256];

    static const uint8_t  waveform_3bit[8][8]; 
    static const uint32_t waveform[50]; 

  public:
    EInk();

    void begin(Mode mode);

    inline void set_display_mode(Mode new_mode) { mode = new_mode; }
    inline Mode get_display_mode() { return mode; }

    void clear_display();
    void update();
    void partial_update();

    void turn_on();
    void turn_off();

    uint8_t read_power_good();

    void clean();
    void clean_fast(uint8_t c, uint8_t rep);
};

#if EINK
  EInk e_ink;
#else
  extern EInk e_ink;
#endif

#endif
