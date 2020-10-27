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

#ifndef __EINK_HPP__
#define __EINK_HPP__

#include "defines.hpp"
#include "noncopyable.hpp"

/**
 * @brief Low level e-Ink display
 * 
 * This class implements the low level methods required to control
 * and access the e-ink display of the InkPlate-6 device.
 * 
 * This is a singleton. It cannot be instanciated elsewhere. It is not 
 * instanciated in the heap. This is reinforced by the C++ construction
 * below. It also cannot be copied through the NonCopyable derivation.
 */

class EInk : NonCopyable
{
  public:
    static const uint16_t WIDTH  = 800; // In pixels
    static const uint16_t HEIGHT = 600; // In pixels
    static const uint16_t BITMAP_SIZE_1BIT = (WIDTH * HEIGHT) >> 3;            // In bytes
    static const uint32_t BITMAP_SIZE_3BIT = ((uint32_t) WIDTH * HEIGHT) >> 1; // In bytes
    static const uint16_t LINE_SIZE_1BIT   = WIDTH >> 3;                       // In bytes
    static const uint16_t LINE_SIZE_3BIT   = WIDTH >> 1;                       // In bytes

    enum PanelMode   { PM_1BIT, PM_3BIT };
    enum PanelState  { OFF, ON };

  private:
    static const uint8_t EINK_ADDRESS = 0x48;
   
    PanelMode  panel_mode;
    PanelState panel_state;

    bool initialized;

    static EInk singleton;
    EInk();  // Private constructor

    void precalculateGamma(uint8_t * c, float gamma);

    void update_1b();
    void update_3b();

    void vscan_start();
    void vscan_end();
    void hscan_start(uint32_t _d = 0);
    
    void pins_z_state();
    void pins_as_outputs();

    uint32_t pin_lut[256];

    static const uint8_t  waveform_3bit[8][8]; 
    static const uint32_t waveform[50]; 
    static const uint8_t  lut2[16];
    static const uint8_t  lutw[16];
    static const uint8_t  lutb[16];

    uint8_t * D_memory4Bit;
    uint8_t * _pBuffer;
    uint8_t * DMemoryNew;
    uint8_t * partial;

    bool block_partial;

  public:

    static inline EInk & get_singleton() noexcept { return singleton; }

    void begin(PanelMode mode);

    inline void       set_panel_state(PanelState s) { panel_state = s; }
    inline PanelState get_panel_state() { return panel_state; }

    inline void       set_panel_mode(PanelMode new_mode) { panel_mode = new_mode; }
    inline PanelMode  get_panel_mode() { return panel_mode; }

    inline bool       is_initialized() { return initialized; }

    void clear_bitmap();
    void update();
    void partial_update();

    void turn_on();
    void turn_off();

    uint8_t read_power_good();

    void clean();
    void clean_fast(uint8_t c, uint8_t rep);
};

#if __EINK__
  EInk & e_ink = EInk::get_singleton();
#else
  extern EInk & e_ink;
#endif

#endif
