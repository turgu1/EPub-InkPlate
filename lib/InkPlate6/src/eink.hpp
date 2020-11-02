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
#include "driver/gpio.h"

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

    typedef uint8_t Bitmap3Bit [BITMAP_SIZE_3BIT];
    typedef uint8_t Bitmap1Bit [BITMAP_SIZE_1BIT];

    enum PanelMode   { PM_1BIT, PM_3BIT };
    enum PanelState  { OFF, ON };

  private:
    static const uint8_t PWRMGR_ADDRESS = 0x48;
   
    PanelState panel_state;
    bool       initialized;
    bool       partial_allowed;


    static EInk singleton;
    EInk();  // Private constructor

    void update_1bit(const Bitmap1Bit & bitmap);
    void update_3bit(const Bitmap3Bit & bitmap);

    void vscan_start();
    void vscan_end();
    void hscan_start(uint32_t d = 0);
    
    void pins_z_state();
    void pins_as_outputs();

    uint32_t pin_lut[256];

    static const uint8_t  WAVEFORM_3BIT[8][8]; 
    static const uint32_t WAVEFORM[50]; 
    static const uint8_t  LUT2[16];
    static const uint8_t  LUTW[16];
    static const uint8_t  LUTB[16];

    static const uint8_t CL    = 0x01;
    static const uint8_t CKV   = 0x01;
    static const uint8_t SPH   = 0x02;
    static const uint8_t LE    = 0x04;

    static const uint32_t DATA = 0x0E8C0030;

    uint8_t * p_buffer;
    Bitmap1Bit * d_memory_new;

    inline void cl_set()    { GPIO.out_w1ts = CL; }
    inline void cl_clear()  { GPIO.out_w1tc = CL; }

    inline void ckv_set()   { GPIO.out1_w1ts.val = CKV; }
    inline void ckv_clear() { GPIO.out1_w1tc.val = CKV; }

    inline void sph_set()   { GPIO.out1_w1ts.val = SPH; }
    inline void sph_clear() { GPIO.out1_w1tc.val = SPH; }

    inline void le_set()    { GPIO.out_w1ts = LE; }
    inline void le_clear()  { GPIO.out_w1tc = LE; }

  public:

    static inline EInk & get_singleton() noexcept { return singleton; }

    void test();
    
    bool initialize();

    inline void       set_panel_state(PanelState s) { panel_state = s; }
    inline PanelState get_panel_state() { return panel_state; }

    inline bool        is_initialized() { return initialized; }

    static inline void clear_bitmap(Bitmap1Bit & bitmap) { memset(&bitmap,   0, sizeof(Bitmap1Bit)); }
    static inline void clear_bitmap(Bitmap3Bit & bitmap) { memset(&bitmap, 255, sizeof(Bitmap3Bit)); }

    inline void update(const Bitmap1Bit & bitmap) { update_1bit(bitmap); }
    inline void update(const Bitmap3Bit & bitmap) { update_3bit(bitmap); }

    void partial_update(const Bitmap1Bit & bitmap);

    void turn_on();
    void turn_off();

    inline void allow_partial() { partial_allowed = true;  }
    inline void block_partial() { partial_allowed = false; }
    inline bool is_partial_allowed() { return partial_allowed; }

    uint8_t read_power_good();

    void clean();
    void clean_fast(uint8_t c, uint8_t rep);

    int8_t read_temperature();
};

#if __EINK__
  EInk & e_ink = EInk::get_singleton();
#else
  extern EInk & e_ink;
#endif

#endif
