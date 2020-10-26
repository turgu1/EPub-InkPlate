/*
eink.cpp
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

#define EINK 1
#include "eink.h"

#include "wire.h"
#include "mcp.h"

#define CL 0x01

#define CL_SET    { GPIO.out_w1ts = CL; }
#define CL_CLEAR  { GPIO.out_w1tc = CL; }

#define CKV 0x01

#define CKV_SET    { GPIO.out1_w1ts.val = CKV; }
#define CKV_CLEAR  { GPIO.out1_w1tc.val = CKV; }

#define SPH 0x02

#define SPH_SET     { GPIO.out1_w1ts.val = SPH; }
#define SPH_CLEAR   { GPIO.out1_w1tc.val = SPH; }

#define LE 0x04

#define LE_SET      { GPIO.out_w1ts = LE; }
#define LE_CLEAR    { GPIO.out_w1tc = LE; }


#define DATA 0x0E8C0030

const uint8_t EInk::waveform_3bit[8][8] = {
  {0, 0, 0, 0, 1, 1, 1, 0}, {1, 2, 2, 2, 1, 1, 1, 0}, {0, 1, 2, 1, 1, 2, 1, 0},
  {0, 2, 1, 2, 1, 2, 1, 0}, {0, 0, 0, 1, 1, 1, 2, 0}, {2, 1, 1, 1, 2, 1, 2, 0},
  {1, 1, 1, 2, 1, 2, 2, 0}, {0, 0, 0, 0, 0, 0, 2, 0} };

const uint32_t EInk::waveform[50] = {
    0x00000008, 0x00000008, 0x00200408, 0x80281888, 0x60A81898, 0x60A8A8A8, 0x60A8A8A8, 0x6068A868, 0x6868A868,
    0x6868A868, 0x68686868, 0x6A686868, 0x5A686868, 0x5A686868, 0x5A586A68, 0x5A5A6A68, 0x5A5A6A68, 0x55566A68,
    0x55565A64, 0x55555654, 0x55555556, 0x55555556, 0x55555556, 0x55555516, 0x55555596, 0x15555595, 0x95955595,
    0x95959595, 0x95949495, 0x94949495, 0x94949495, 0xA4949494, 0x9494A4A4, 0x84A49494, 0x84948484, 0x84848484,
    0x84848484, 0x84848484, 0xA5A48484, 0xA9A4A4A8, 0xA9A8A8A8, 0xA5A9A9A4, 0xA5A5A5A4, 0xA1A5A5A1, 0xA9A9A9A9,
    0xA9A9A9A9, 0xA9A9A9A9, 0xA9A9A9A9, 0x15151515, 0x11111111 };

EInk::EInk()
{
  for (uint32_t i = 0; i < 256; ++i) {
    pin_LUT[i] =  ((i & 0b00000011) << 4) | (((i & 0b00001100) >> 2) << 18) | (((i & 0b00010000) >> 4) << 23) |
                  (((i & 0b11100000) >> 5) << 25);
  }
}

void 
EInk::begin(Mode mode)
{
    if (initialized) return;

    set_display_mode(mode);

    Wire::begin();
    mcp.WAKEUP_SET();
    delay(1);
    Wire::begin_transmission(EINK_ADDRESS);
    Wire::write(0x09);
    Wire::write(0b00011011); // Power up seq.
    Wire::write(0b00000000); // Power up delay (3mS per rail)
    Wire::write(0b00011011); // Power down seq.
    Wire::write(0b00000000); // Power down delay (6mS per rail)
    Wire::end_transmission();
    delay(1);
    mcp.WAKEUP_CLEAR();

    mcp.begin(mcpRegsInt);
    mcp.pin_mode(MCP::VCOM,   MCP::OUTPUT);
    mcp.pin_mode(MCP::PWRUP,  MCP::OUTPUT);
    mcp.pin_mode(MCP::WAKEUP, MCP::OUTPUT);
    mcp.pin_mode(MCP::GPIO0_ENABLE, MCP::OUTPUT);
    mcp.digital_write(MCP::GPIO0_ENABLE, HIGH);

    // CONTROL PINS
    pin_mode( 0, OUTPUT);
    pin_mode( 2, OUTPUT);
    pin_mode(32, OUTPUT);
    pin_mode(33, OUTPUT);
    mcp.pin_mode(MCP::OE,   MCP::OUTPUT);
    mcp.pin_mode(MCP::GMOD, MCP::OUTPUT);
    mcp.pin_mode(MCP::SPV,  MCP::OUTPUT);

    // DATA PINS
    pin_mode( 4, OUTPUT); // D0
    pin_mode( 5, OUTPUT);
    pin_mode(18, OUTPUT);
    pin_mode(19, OUTPUT);
    pin_mode(23, OUTPUT);
    pin_mode(25, OUTPUT);
    pin_mode(26, OUTPUT);
    pin_mode(27, OUTPUT); // D7

    // TOUCHPAD PINS
    mcp.pin_mode(MCP::TOUCH_0, MCP::INPUT);
    mcp.pin_mode(MCP::TOUCH_1, MCP::INPUT);
    mcp.pin_mode(MCP::TOUCH_2, MCP::INPUT);

    // Battery voltage Switch MOSFET
    mcp.pin_mode(MCP::BATTERY_SWITCH, OUTPUT);

    DMemoryNew   = (uint8_t *) ps_malloc(BITMAP_SIZE_1BIT);
    _partial     = (uint8_t *) ps_malloc(BITMAP_SIZE_1BIT);
    _pBuffer     = (uint8_t *) ps_malloc(120000);
    D_memory4Bit = (uint8_t *) ps_malloc(240000);

    if ((DMemoryNew == nullptr) || (_partial == nullptr) || (_pBuffer == nullptr) || (D_memory4Bit == nullptr)) {
      do {
        delay(100);
      } while (true);
    }

    memset(DMemoryNew, 0, BITMAP_SIZE_1BIT);
    memset(_partial,   0, BITMAP_SIZE_1BIT);
    memset(_pBuffer, 0, 120000);
    memset(D_memory4Bit, 255, 240000);

    initialized = true;
}

void 
EInk::clear_display()
{
  if      (get_display_mode() == 0) memset(_partial, 0, BITMAP_SIZE_1BIT);
  else if (get_display_mode() == 1) memset(D_memory4Bit, 255, 240000);
}

void 
EInk::update()
{
  if      (get_display_mode() == 0) update_1b();
  else if (get_display_mode() == 1) update_3b();
}

void 
EInk::update_1b()
{
    memcpy(DMemoryNew, _partial, BITMAP_SIZE_1BIT);

    uint32_t _send;
    uint8_t data;
    uint8_t dram;

    turn_on();
    clean_fast(0,  1);
    clean_fast(1, 21);
    clean_fast(2,  1);
    clean_fast(0, 12);
    clean_fast(2,  1);
    clean_fast(1, 21);
    clean_fast(2,  1);
    clean_fast(0, 12);

    for (int k = 0; k < 4; ++k) {
        uint8_t * DMemoryNewPtr = DMemoryNew + BITMAP_SIZE_1BIT - 1;
        vscan_start();
        for (int i = 0; i < 600; ++i)
        {
            dram = *(DMemoryNewPtr--);
            data = LUTB[dram >> 4];
            _send = pin_LUT[data];
            hscan_start(_send);
            data = LUTB[dram & 0x0F];
            _send = pin_LUT[data];
            GPIO.out_w1ts = (_send) | CL;
            GPIO.out_w1tc = DATA | CL;

            for (int j = 0; j < 99; ++j)
            {
                dram = *(DMemoryNewPtr--);
                data = LUTB[dram >> 4];
                _send = pin_LUT[data];
                GPIO.out_w1ts = (_send) | CL;
                GPIO.out_w1tc = DATA | CL;
                data = LUTB[dram & 0x0F];
                _send = pin_LUT[data];
                GPIO.out_w1ts = (_send) | CL;
                GPIO.out_w1tc = DATA | CL;
            }
            GPIO.out_w1ts = (_send) | CL;
            GPIO.out_w1tc = DATA | CL;
            vscan_end();
        }
        delayMicroseconds(230);
    }

    uint16_t _pos = 59999;
    vscan_start();
    for (int i = 0; i < 600; ++i)
    {
        dram = *(DMemoryNew + _pos);
        data = LUT2[dram >> 4];
        _send = pin_LUT[data];
        hscan_start(_send);
        data = LUT2[dram & 0x0F];
        _send = pin_LUT[data];
        GPIO.out_w1ts = (_send) | CL;
        GPIO.out_w1tc = DATA | CL;
        _pos--;
        for (int j = 0; j < 99; ++j)
        {
            dram = *(DMemoryNew + _pos);
            data = LUT2[dram >> 4];
            _send = pin_LUT[data];
            GPIO.out_w1ts = (_send) | CL;
            GPIO.out_w1tc = DATA | CL;
            data = LUT2[dram & 0x0F];
            _send = pin_LUT[data];
            GPIO.out_w1ts = (_send) | CL;
            GPIO.out_w1tc = DATA | CL;
            _pos--;
        }
        GPIO.out_w1ts = (_send) | CL;
        GPIO.out_w1tc = DATA | CL;
        vscan_end();
    }
    delayMicroseconds(230);

    vscan_start();
    for (int i = 0; i < HEIGHT; ++i)
    {
        dram = *(DMemoryNew + _pos);
        data = 0;
        _send = pin_LUT[data];
        hscan_start(_send);
        data = 0;
        GPIO.out_w1ts = (_send) | CL;
        GPIO.out_w1tc = DATA | CL;
        for (int j = 0; j < 99; ++j)
        {
            GPIO.out_w1ts = (_send) | CL;
            GPIO.out_w1tc = DATA | CL;
            GPIO.out_w1ts = (_send) | CL;
            GPIO.out_w1tc = DATA | CL;
        }
        GPIO.out_w1ts = (_send) | CL;
        GPIO.out_w1tc = DATA | CL;
        vscan_end();
    }
    delayMicroseconds(230);

    vscan_start();
    turn_off();
    _blockPartial = 0;
}

void 
EInk::update_3b()
{
  turn_on();
  clean_fast(0,  1);
  clean_fast(1, 21);
  clean_fast(2,  1);
  clean_fast(0, 12);
  clean_fast(2,  1);
  clean_fast(1, 21);
  clean_fast(2,  1);
  clean_fast(0, 12);

  for (int k = 0; k < 8; ++k) {
    uint8_t *dp = D_memory4Bit + BITMAP_SIZE_3BIT - 1;
    uint32_t _send;
    uint8_t pix1;
    uint8_t pix2;
    uint8_t pix3;
    uint8_t pix4;
    uint8_t pixel;
    uint8_t pixel2;

    vscan_start();
    for (int i = 0; i < HEIGHT; ++i) {
      pixel = 0;
      pixel2 = 0;
      pix1 = *(dp--);
      pix2 = *(dp--);
      pix3 = *(dp--);
      pix4 = *(dp--);
      pixel |= (waveform3Bit[pix1 & 0x07][k] << 6) | (waveform3Bit[(pix1 >> 4) & 0x07][k] << 4) |
               (waveform3Bit[pix2 & 0x07][k] << 2) | (waveform3Bit[(pix2 >> 4) & 0x07][k] << 0);
      pixel2 |= (waveform3Bit[pix3 & 0x07][k] << 6) | (waveform3Bit[(pix3 >> 4) & 0x07][k] << 4) |
                (waveform3Bit[pix4 & 0x07][k] << 2) | (waveform3Bit[(pix4 >> 4) & 0x07][k] << 0);

      _send = pin_LUT[pixel];
      hscan_start(_send);
      _send = pin_LUT[pixel2];
      GPIO.out_w1ts = (_send) | CL;
      GPIO.out_w1tc = DATA | CL;

      for (int j = 0; j < 99; ++j) {
        pixel = 0;
        pixel2 = 0;
        pix1 = *(dp--);
        pix2 = *(dp--);
        pix3 = *(dp--);
        pix4 = *(dp--);
        pixel |= (waveform3Bit[pix1 & 0x07][k] << 6) | (waveform3Bit[(pix1 >> 4) & 0x07][k] << 4) |
                 (waveform3Bit[pix2 & 0x07][k] << 2) | (waveform3Bit[(pix2 >> 4) & 0x07][k] << 0);
        pixel2 |= (waveform3Bit[pix3 & 0x07][k] << 6) | (waveform3Bit[(pix3 >> 4) & 0x07][k] << 4) |
                  (waveform3Bit[pix4 & 0x07][k] << 2) | (waveform3Bit[(pix4 >> 4) & 0x07][k] << 0);

        _send = pin_LUT[pixel];
        GPIO.out_w1ts = (_send) | CL;
        GPIO.out_w1tc = DATA | CL;

        _send = pin_LUT[pixel2];
        GPIO.out_w1ts = (_send) | CL;
        GPIO.out_w1tc = DATA | CL;
      }
      GPIO.out_w1ts = (_send) | CL;
      GPIO.out_w1tc = DATA | CL;
      vscan_end();
    }
    delayMicroseconds(230);
  }

  clean_fast(3, 1);
  vscan_start();
  turn_off();
}

void 
EInk::partial_update()
{
  if (getDisplayMode() == 1) return;
  if (_blockPartial == 1) display1b();

  uint16_t _pos = 59999;
  uint32_t _send;
  uint8_t data = 0;
  uint8_t diffw, diffb;
  uint32_t n = 119999;

  for (int i = 0; i < HEIGHT; ++i) {
    for (int j = 0; j < 100; ++j) {
      diffw = *(DMemoryNew + _pos) & ~*(_partial + _pos);
      diffb = ~*(DMemoryNew + _pos) & *(_partial + _pos);
      _pos--;
      *(_pBuffer + n) = LUTW[diffw >> 4] & (LUTB[diffb >> 4]);
      n--;
      *(_pBuffer + n) = LUTW[diffw & 0x0F] & (LUTB[diffb & 0x0F]);
      n--;
    }
  }

  turn_on();
  for (int k = 0; k < 5; ++k) {
    vscan_start();
    n = 119999;
    for (int i = 0; i < HEIGHT; ++i) {
      data = *(_pBuffer + n);
      _send = pin_LUT[data];
      hscan_start(_send);
      n--;
      for (int j = 0; j < 199; ++j) {
        data = *(_pBuffer + n);
        _send = pin_LUT[data];
        GPIO.out_w1ts = _send | CL;
        GPIO.out_w1tc = DATA | CL;
        n--;
      }
      GPIO.out_w1ts = _send | CL;
      GPIO.out_w1tc = DATA | CL;
      vscan_end();
    }
    delayMicroseconds(230);
  }

  clean_fast(2, 2);
  clean_fast(3, 1);
  vscan_start();
  turn_off();

  memcpy(DMemoryNew, _partial, 60000);
}

void 
EInk::clean()
{
  turn_on();
  int m = 0;
  clean_fast(0, 1);
  m++; clean_fast((waveform[m] >> 30) & 3,  8);
  m++; clean_fast((waveform[m] >> 24) & 3,  1);
  m++; clean_fast((waveform[m]        & 3,  8);
  m++; clean_fast((waveform[m] >>  6) & 3,  1);
  m++; clean_fast((waveform[m] >> 30) & 3, 10);
}

void 
EInk::clean_fast(uint8_t c, uint8_t rep)
{
  turn_on();
  uint8_t data = 0;
 
  if      (c == 0) data = 0b10101010;
  else if (c == 1) data = 0b01010101;
  else if (c == 2) data = 0b00000000;
  else if (c == 3) data = 0b11111111;

  uint32_t _send = pin_LUT[data];

  for (int k = 0; k < rep; ++k) {
    vscan_start();
    for (int i = 0; i < HEIGHT; ++i) {
      hscan_start(_send);
      GPIO.out_w1ts = (_send) | CL;
      GPIO.out_w1tc = DATA | CL;
      for (int j = 0; j < 99; ++j) {
        GPIO.out_w1ts = (_send) | CL;
        GPIO.out_w1tc = DATA | CL;
        GPIO.out_w1ts = (_send) | CL;
        GPIO.out_w1tc = DATA | CL;
      }
      GPIO.out_w1ts = (_send) | CL;
      GPIO.out_w1tc = DATA | CL;
      vscan_end();
    }
    delayMicroseconds(230);
  }
}

// Turn off epaper power supply and put all digital IO pins in high Z state
void 
EInk::turn_off()
{
  if (get_panel_state() == 0) return;

  OE_CLEAR;
  GMOD_CLEAR;
  GPIO.out &= ~(DATA | LE | CL);
  CKV_CLEAR;
  SPH_CLEAR;
  SPV_CLEAR;

  VCOM_CLEAR;
  delay(6);
  PWRUP_CLEAR;
  WAKEUP_CLEAR;

  unsigned long timer = millis();

  do {
    delay(1);
  } while ((readPowerGood() != 0) && (millis() - timer) < 250);

  pins_z_state();
  set_panel_state(0);
}

// Turn on supply for epaper display (TPS65186) [+15 VDC, -15VDC, +22VDC, -20VDC, +3.3VDC, VCOM]
void EInk::turn_on()
{
  if (get_panel_state() == 1) return;

  WAKEUP_SET;
  delay(1);
  PWRUP_SET;

  // Enable all rails
  Wire::begin_transmission(EINK_ADDRESS);
  Wire::write(0x01);
  Wire::write(0b00111111);
  Wire::end_transmission();
  pinsAsOutputs();
  LE_CLEAR;
  OE_CLEAR;
  CL_CLEAR;
  SPH_SET;
  GMOD_SET;
  SPV_SET;
  CKV_CLEAR;
  OE_CLEAR;
  VCOM_SET;

  unsigned long timer = millis();

  do {
    delay(1);
  } while ((readPowerGood() != PWR_GOOD_OK) && (millis() - timer) < 250);

  if ((millis() - timer) >= 250) {
    WAKEUP_CLEAR;
    VCOM_CLEAR;
    PWRUP_CLEAR;
    return;
  }

  OE_SET;
  set_panel_state(1);
}

uint8_t EInk::readPowerGood()
{
  Wire::begin_transmission(EINK_ADDRESS);
  Wire::write(0x0F);
  Wire::end_transmission();

  Wire::requestFrom(EINK_ADDRESS, 1);
  return Wire::read();
}

// LOW LEVEL FUNCTIONS

void EInk::vscan_start()
{
  CKV_SET;    delayMicroseconds( 7);
  SPV_CLEAR;  delayMicroseconds(10);
  CKV_CLEAR;  delayMicroseconds( 0);
  CKV_SET;    delayMicroseconds( 8);
  SPV_SET;    delayMicroseconds(10);
  CKV_CLEAR;  delayMicroseconds( 0);
  CKV_SET;    delayMicroseconds(18);
  CKV_CLEAR;  delayMicroseconds( 0);
  CKV_SET;    delayMicroseconds(18);
  CKV_CLEAR;  delayMicroseconds( 0);
  CKV_SET;
}

void EInk::hscan_start(uint32_t _d)
{
  SPH_CLEAR;
  GPIO.out_w1ts = (_d) | CL;
  GPIO.out_w1tc = DATA | CL;
  SPH_SET;
  CKV_SET;
}

void EInk::vscan_end()
{
  CKV_CLEAR;
  LE_SET;
  LE_CLEAR;
  delayMicroseconds(0);
}

void EInk::pins_z_state()
{
  pin_mode( 0, INPUT);
  pin_mode( 2, INPUT);
  pin_mode(32, INPUT);
  pin_mode(33, INPUT);

  mcp.pin_mode(MCP::OE,   MCP::INPUT);
  mcp.pin_mode(MCP::GMOD, MCP::INPUT);
  mcp.pin_mode(MCP::SPV,  MCP::INPUT);

  pin_mode( 4, INPUT);
  pin_mode( 5, INPUT);
  pin_mode(18, INPUT);
  pin_mode(19, INPUT);
  pin_mode(23, INPUT);
  pin_mode(25, INPUT);
  pin_mode(26, INPUT);
  pin_mode(27, INPUT);
}

void EInk::pins_as_outputs()
{
  pin_mode( 0, OUTPUT);
  pin_mode( 2, OUTPUT);
  pin_mode(32, OUTPUT);
  pin_mode(33, OUTPUT);

  mcp.pin_mode(MCP::OE,   MCP::OUTPUT);
  mcp.pin_mode(MCP::GMOD, MCP::OUTPUT);
  mcp.pin_mode(MCP::SPV,  MCP::OUTPUT);

  pin_mode( 4, OUTPUT);
  pin_mode( 5, OUTPUT);
  pin_mode(18, OUTPUT);
  pin_mode(19, OUTPUT);
  pin_mode(23, OUTPUT);
  pin_mode(25, OUTPUT);
  pin_mode(26, OUTPUT);
  pin_mode(27, OUTPUT);
}