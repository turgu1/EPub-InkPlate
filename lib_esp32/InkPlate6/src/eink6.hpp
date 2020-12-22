/*
eink.h
Inkplate 6 ESP-IDF

Modified by Guy Turcotte 
November 12, 2020

from the Arduino Library:

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

#ifndef __EINK6_HPP__
#define __EINK6_HPP__

#include <cinttypes>
#include <cstring>

#include "noncopyable.hpp"
#include "driver/gpio.h"
#include "mcp.hpp"
#include "eink.hpp"

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

class EInk6 : EInk, NonCopyable
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

    static inline EInk6 & get_singleton() noexcept { return singleton; }

  private:
    static constexpr char const * TAG = "EInk6";

    static EInk6 singleton;

    static const uint8_t PWRMGR_ADDRESS = 0x48;
    static const uint8_t PWR_GOOD_OK    = 0b11111010;

    class Bitmap1Bit6 : public Bitmap1Bit {
      private:
        uint8_t data[BITMAP_SIZE_1BIT];
      public:
        Bitmap1Bit6() : Bitmap1Bit(WIDTH, HEIGHT, BITMAP_SIZE_1BIT) {}

        void                clear() { memset(data, get_init_value(), get_data_size()); }        
        inline uint8_t * get_data() { return data; };
    };

    class Bitmap3Bit6 : public Bitmap3Bit {
      private:
        uint8_t data[BITMAP_SIZE_3BIT];
      public:
        Bitmap3Bit6() : Bitmap3Bit(WIDTH, HEIGHT, BITMAP_SIZE_3BIT) {}
            
        void                clear() { memset(data, get_init_value(), get_data_size()); }
        inline uint8_t * get_data() { return data; };
    };

    EInk6() : EInk()
      { }  // Private constructor

    void update_1bit(const Bitmap1Bit & bitmap);
    void update_3bit(const Bitmap3Bit & bitmap);

    void vscan_start();
    void vscan_end();
    void hscan_start(uint32_t d = 0);
    
    void pins_z_state();
    void pins_as_outputs();

    void turn_on();
    void turn_off();

    void clean_fast(uint8_t c, uint8_t rep);

    uint8_t read_power_good();

    inline void  set_panel_state(PanelState s) { panel_state = s; }

    inline void allow_partial() { partial_allowed = true;  }
    inline void block_partial() { partial_allowed = false; }
    inline bool is_partial_allowed() { return partial_allowed; }

    static const uint32_t PIN_LUT[256];

    static const uint8_t  WAVEFORM_3BIT[8][8]; 
    static const uint32_t WAVEFORM[50]; 
    static const uint8_t  LUT2[16];
    static const uint8_t  LUTW[16];
    static const uint8_t  LUTB[16];

    static const uint32_t CL   = 0x01;
    static const uint32_t CKV  = 0x01;
    static const uint32_t SPH  = 0x02;
    static const uint32_t LE   = 0x04;

    static const uint32_t DATA = 0x0E8C0030;

    uint8_t    * p_buffer;
    Bitmap1Bit * d_memory_new;

    const MCP::Pin OE             = MCP::Pin::IOPIN_0;
    const MCP::Pin GMOD           = MCP::Pin::IOPIN_1;
    const MCP::Pin SPV            = MCP::Pin::IOPIN_2;
    const MCP::Pin WAKEUP         = MCP::Pin::IOPIN_3;
    const MCP::Pin PWRUP          = MCP::Pin::IOPIN_4;
    const MCP::Pin VCOM           = MCP::Pin::IOPIN_5;
    const MCP::Pin GPIO0_ENABLE   = MCP::Pin::IOPIN_8;
  
    inline void cl_set()    { GPIO.out_w1ts = CL; }
    inline void cl_clear()  { GPIO.out_w1tc = CL; }

    inline void ckv_set()   { GPIO.out1_w1ts.val = CKV; }
    inline void ckv_clear() { GPIO.out1_w1tc.val = CKV; }

    inline void sph_set()   { GPIO.out1_w1ts.val = SPH; }
    inline void sph_clear() { GPIO.out1_w1tc.val = SPH; }

    inline void le_set()    { GPIO.out_w1ts = LE; }
    inline void le_clear()  { GPIO.out_w1tc = LE; }

    inline void oe_set()       { mcp.digital_write(OE,     MCP::SignalLevel::HIGH); }
    inline void oe_clear()     { mcp.digital_write(OE,     MCP::SignalLevel::LOW ); }

    inline void gmod_set()     { mcp.digital_write(GMOD,   MCP::SignalLevel::HIGH); }
    inline void gmod_clear()   { mcp.digital_write(GMOD,   MCP::SignalLevel::LOW ); }

    inline void spv_set()      { mcp.digital_write(SPV,    MCP::SignalLevel::HIGH); }
    inline void spv_clear()    { mcp.digital_write(SPV,    MCP::SignalLevel::LOW ); }

    inline void wakeup_set()   { mcp.digital_write(WAKEUP, MCP::SignalLevel::HIGH); }
    inline void wakeup_clear() { mcp.digital_write(WAKEUP, MCP::SignalLevel::LOW ); }

    inline void pwrup_set()    { mcp.digital_write(PWRUP,  MCP::SignalLevel::HIGH); }
    inline void pwrup_clear()  { mcp.digital_write(PWRUP,  MCP::SignalLevel::LOW ); }

    inline void vcom_set()     { mcp.digital_write(VCOM,   MCP::SignalLevel::HIGH); }
    inline void vcom_clear()   { mcp.digital_write(VCOM,   MCP::SignalLevel::LOW ); }

  public:

    inline PanelState get_panel_state() { return panel_state; }
    inline bool        is_initialized() { return initialized; }

    static inline void clear_bitmap(Bitmap1Bit & bitmap) { memset(&bitmap,    0, sizeof(Bitmap1Bit)); }
    static inline void clear_bitmap(Bitmap3Bit & bitmap) { memset(&bitmap, 0x77, sizeof(Bitmap3Bit)); }

    // All the following methods are protecting the I2C device trough
    // the Wire::enter() and Wire::leave() methods. These are implementing a
    // Mutex semaphore access control.
    //
    // If you ever add public methods, you *MUST* consider adding calls to Wire::enter()
    // and Wire::leave() and insure no deadlock will happen... or modifu the mutex to use
    // a recursive mutex.

    bool setup();

    inline void update(const Bitmap1Bit & bitmap) { update_1bit(bitmap); }
    inline void update(const Bitmap3Bit & bitmap) { update_3bit(bitmap); }

    void partial_update(const Bitmap1Bit & bitmap);

    void clean();

    int8_t  read_temperature();
};

#if __EINK6__
  EInk6 & e_ink = EInk6::get_singleton();
#else
  extern EInk6 & e_ink;
#endif

#endif
