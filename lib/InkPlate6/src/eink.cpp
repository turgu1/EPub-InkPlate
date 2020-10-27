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

#define __EINK__ 1
#include "eink.hpp"

#include "wire.hpp"
#include "mcp.hpp"
#include "esp.hpp"

#include "driver/gpio.h"

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

EInk EInk::singleton;

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

const uint8_t EInk::lut2[16] = {
  0xAA, 0xA9, 0xA6, 0xA5, 0x9A, 0x99, 0x96, 0x95,
  0x6A, 0x69, 0x66, 0x65, 0x5A, 0x59, 0x56, 0x55 };

const uint8_t EInk::lutw[16] = {
  0xFF, 0xFE, 0xFB, 0xFA, 0xEF, 0xEE, 0xEB, 0xEA,
  0xBF, 0xBE, 0xBB, 0xBA, 0xAF, 0xAE, 0xAB, 0xAA };

const uint8_t EInk::lutb[16] = {
  0xFF, 0xFD, 0xF7, 0xF5, 0xDF, 0xDD, 0xD7, 0xD5,
  0x7F, 0x7D, 0x77, 0x75, 0x5F, 0x5D, 0x57, 0x55 };

EInk::EInk() : 
  panel_mode(PM_1BIT), 
  panel_state(OFF), 
  initialized(false)
{
  for (uint32_t i = 0; i < 256; ++i) {
    pin_lut[i] =  ((i & 0b00000011) << 4) | (((i & 0b00001100) >> 2) << 18) | (((i & 0b00010000) >> 4) << 23) |
                  (((i & 0b11100000) >> 5) << 25);
  }
}

void 
EInk::begin(PanelMode mode)
{
  if (initialized) return;

  set_panel_mode(mode);

  wire.begin();
  mcp.WAKEUP_SET();
  ESP::delay(1);
  wire.begin_transmission(EINK_ADDRESS);
  wire.write(0x09);
  wire.write(0b00011011); // Power up seq.
  wire.write(0b00000000); // Power up delay (3mS per rail)
  wire.write(0b00011011); // Power down seq.
  wire.write(0b00000000); // Power down delay (6mS per rail)
  wire.end_transmission();
  ESP::delay(1);
  mcp.WAKEUP_CLEAR();

  mcp.begin();
  mcp.set_direction(MCP::VCOM,   MCP::OUTPUT);
  mcp.set_direction(MCP::PWRUP,  MCP::OUTPUT);
  mcp.set_direction(MCP::WAKEUP, MCP::OUTPUT);
  mcp.set_direction(MCP::GPIO0_ENABLE, MCP::OUTPUT);
  mcp.digital_write(MCP::GPIO0_ENABLE, HIGH);

  // CONTROL PINS
  gpio_set_direction(GPIO_NUM_0,  GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_2,  GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_33, GPIO_MODE_OUTPUT);
  mcp.set_direction(MCP::OE,      MCP::OUTPUT);
  mcp.set_direction(MCP::GMOD,    MCP::OUTPUT);
  mcp.set_direction(MCP::SPV,     MCP::OUTPUT);

  // DATA PINS
  gpio_set_direction(GPIO_NUM_4,  GPIO_MODE_OUTPUT); // D0
  gpio_set_direction(GPIO_NUM_5,  GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_23, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_26, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT); // D7

  // TOUCHPAD PINS
  mcp.set_direction(MCP::TOUCH_0, MCP::INPUT);
  mcp.set_direction(MCP::TOUCH_1, MCP::INPUT);
  mcp.set_direction(MCP::TOUCH_2, MCP::INPUT);

  // Battery voltage Switch MOSFET
  mcp.set_direction(MCP::BATTERY_SWITCH, MCP::OUTPUT);

  DMemoryNew   = (uint8_t *) ESP::ps_malloc(BITMAP_SIZE_1BIT);
  partial      = (uint8_t *) ESP::ps_malloc(BITMAP_SIZE_1BIT);
  _pBuffer     = (uint8_t *) ESP::ps_malloc(120000);
  D_memory4Bit = (uint8_t *) ESP::ps_malloc(240000);

  if ((DMemoryNew   == nullptr) || 
      (partial      == nullptr) || 
      (_pBuffer     == nullptr) || 
      (D_memory4Bit == nullptr)) {
    do {
      ESP::delay(100);
    } while (true);
  }

  memset(DMemoryNew, 0, BITMAP_SIZE_1BIT);
  memset(partial,    0, BITMAP_SIZE_1BIT);
  memset(_pBuffer,   0, 120000);

  memset(D_memory4Bit, 255, 240000);

  initialized = true;
}

void 
EInk::clear_display()
{
  if      (get_panel_mode() == PM_1BIT) memset(partial, 0, BITMAP_SIZE_1BIT);
  else if (get_panel_mode() == PM_3BIT) memset(D_memory4Bit, 255, 240000);
}

void 
EInk::update()
{
  if      (get_panel_mode() == PM_1BIT) update_1b();
  else if (get_panel_mode() == PM_3BIT) update_3b();
}

void 
EInk::update_1b()
{
  memcpy(DMemoryNew, partial, BITMAP_SIZE_1BIT);

  uint32_t send;
  uint8_t  data;
  uint8_t  dram;

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

    for (int i = 0; i < HEIGHT; ++i) {
      dram = *(DMemoryNewPtr--);
      data = lutb[dram >> 4];
      send = pin_lut[data];
      hscan_start(send);
      data = lutb[dram & 0x0F];
      send = pin_lut[data];
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;

      for (int j = 0; j < 99; ++j) {   // ????
        dram = *(DMemoryNewPtr--);
        data = lutb[dram >> 4];
        send = pin_lut[data];
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;
        data = lutb[dram & 0x0F];
        send = pin_lut[data];
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;
      }

      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
      vscan_end();
    }
    ESP::delay_microseconds(230);
  }

  uint16_t pos = 59999;
  vscan_start();

  for (int i = 0; i < 600; ++i) {
    dram = *(DMemoryNew + pos);
    data = lut2[dram >> 4];
    send = pin_lut[data];
    hscan_start(send);
    data = lut2[dram & 0x0F];
    send = pin_lut[data];
    GPIO.out_w1ts = send | CL;
    GPIO.out_w1tc = DATA | CL;
    pos--;
    
    for (int j = 0; j < 99; ++j) {
      dram = *(DMemoryNew + pos);
      data = lut2[dram >> 4];
      send = pin_lut[data];
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
      data = lut2[dram & 0x0F];
      send = pin_lut[data];
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
      pos--;
    }

    GPIO.out_w1ts = send | CL;
    GPIO.out_w1tc = DATA | CL;
    vscan_end();
  }
  ESP::delay_microseconds(230);

  vscan_start();

  for (int i = 0; i < HEIGHT; ++i) {
    dram = *(DMemoryNew + pos);
    data = 0;
    send = pin_lut[data];
    hscan_start(send);
    data = 0;
    GPIO.out_w1ts = send | CL;
    GPIO.out_w1tc = DATA | CL;

    for (int j = 0; j < 99; ++j) {  // ????
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
    }

    GPIO.out_w1ts = send | CL;
    GPIO.out_w1tc = DATA | CL;
    vscan_end();
  }

  ESP::delay_microseconds(230);

  vscan_start();
  turn_off();
  block_partial = false;
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
    uint32_t send;
    uint8_t  pix1;
    uint8_t  pix2;
    uint8_t  pix3;
    uint8_t  pix4;
    uint8_t  pixel;
    uint8_t  pixel2;

    vscan_start();
    for (int i = 0; i < HEIGHT; ++i) {
      pixel  = 0;
      pixel2 = 0;
      pix1   = *(dp--);
      pix2   = *(dp--);
      pix3   = *(dp--);
      pix4   = *(dp--);
      pixel |= (waveform_3bit[pix1 & 0x07][k] << 6) | (waveform_3bit[(pix1 >> 4) & 0x07][k] << 4) |
               (waveform_3bit[pix2 & 0x07][k] << 2) | (waveform_3bit[(pix2 >> 4) & 0x07][k] << 0);
      pixel2 |= (waveform_3bit[pix3 & 0x07][k] << 6) | (waveform_3bit[(pix3 >> 4) & 0x07][k] << 4) |
                (waveform_3bit[pix4 & 0x07][k] << 2) | (waveform_3bit[(pix4 >> 4) & 0x07][k] << 0);

      send = pin_lut[pixel];
      hscan_start(send);
      send = pin_lut[pixel2];
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;

      for (int j = 0; j < 99; ++j) {
        pixel  = 0;
        pixel2 = 0;
        pix1   = *(dp--);
        pix2   = *(dp--);
        pix3   = *(dp--);
        pix4   = *(dp--);

        pixel |= (waveform_3bit[pix1 & 0x07][k] << 6) | (waveform_3bit[(pix1 >> 4) & 0x07][k] << 4) |
                 (waveform_3bit[pix2 & 0x07][k] << 2) | (waveform_3bit[(pix2 >> 4) & 0x07][k] << 0);
        pixel2 |= (waveform_3bit[pix3 & 0x07][k] << 6) | (waveform_3bit[(pix3 >> 4) & 0x07][k] << 4) |
                  (waveform_3bit[pix4 & 0x07][k] << 2) | (waveform_3bit[(pix4 >> 4) & 0x07][k] << 0);

        send = pin_lut[pixel];
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;

        send = pin_lut[pixel2];
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;
      }

      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
      vscan_end();
    }
    ESP::delay_microseconds(230);
  }

  clean_fast(3, 1);
  vscan_start();
  turn_off();
}

void 
EInk::partial_update()
{
  if (get_panel_mode() == PM_1BIT) return;
  if (block_partial) update_1b();

  uint32_t n    = 119999;
  uint32_t send;
  uint16_t pos  = 59999;
  uint8_t  data = 0;
  uint8_t  diffw, diffb;

  for (int i = 0; i < HEIGHT; ++i) {
    for (int j = 0; j < 100; ++j) {
      diffw =  *(DMemoryNew + pos) & ~*(partial + pos);
      diffb = ~*(DMemoryNew + pos) &  *(partial + pos);
      pos--;
      *(_pBuffer + n) = lutw[diffw >> 4] & (lutb[diffb >> 4]);
      n--;
      *(_pBuffer + n) = lutw[diffw & 0x0F] & (lutb[diffb & 0x0F]);
      n--;
    }
  }

  turn_on();

  for (int k = 0; k < 5; ++k) {
    vscan_start();
    n = 119999;

    for (int i = 0; i < HEIGHT; ++i) {
      data = *(_pBuffer + n);
      send = pin_lut[data];
      hscan_start(send);
      n--;

      for (int j = 0; j < 199; ++j) {
        data = *(_pBuffer + n);
       send = pin_lut[data];
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;
        n--;
      }

      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
      vscan_end();
    }
    ESP::delay_microseconds(230);
  }

  clean_fast(2, 2);
  clean_fast(3, 1);
  vscan_start();
  turn_off();

  memcpy(DMemoryNew, partial, 60000);
}

void 
EInk::clean()
{
  turn_on();
  int m = 0;
  clean_fast(0, 1);
  m++; clean_fast((waveform[m] >> 30) & 3,  8);
  m++; clean_fast((waveform[m] >> 24) & 3,  1);
  m++; clean_fast( waveform[m]        & 3,  8);
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

  uint32_t send = pin_lut[data];

  for (int k = 0; k < rep; ++k) {
    vscan_start();

    for (int i = 0; i < HEIGHT; ++i) {
      hscan_start(send);
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;

      for (int j = 0; j < 99; ++j) {
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;
      }
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
      vscan_end();
    }

    ESP::delay_microseconds(230);
  }
}

// Turn off epaper power supply and put all digital IO pins in high Z state
void 
EInk::turn_off()
{
  if (get_panel_state() == OFF) return;

  mcp.OE_CLEAR();
  mcp.GMOD_CLEAR();
  GPIO.out &= ~(DATA | LE | CL);
  CKV_CLEAR;
  SPH_CLEAR;
  mcp.SPV_CLEAR();

  mcp.VCOM_CLEAR();
  ESP::delay(6);
  mcp.PWRUP_CLEAR();
  mcp.WAKEUP_CLEAR();

  unsigned long timer = ESP::millis();

  do {
    ESP::delay(1);
  } while ((read_power_good() != 0) && (ESP::millis() - timer) < 250);

  pins_z_state();
  set_panel_state(OFF);
}

// Turn on supply for epaper display (TPS65186) [+15 VDC, -15VDC, +22VDC, -20VDC, +3.3VDC, VCOM]
void EInk::turn_on()
{
  if (get_panel_state() == ON) return;

  mcp.WAKEUP_SET();
  ESP::delay(1);
  mcp.PWRUP_SET();

  // Enable all rails
  wire.begin_transmission(EINK_ADDRESS);
  wire.write(0x01);
  wire.write(0b00111111);
  wire.end_transmission();

  pins_as_outputs();

  LE_CLEAR;
  mcp.OE_CLEAR();
  CL_CLEAR;
  SPH_SET;
  mcp.GMOD_SET();
  mcp.SPV_SET();
  CKV_CLEAR;
  mcp.OE_CLEAR();
  mcp.VCOM_SET();

  unsigned long timer = ESP::millis();

  do {
    ESP::delay(1);
  } while ((read_power_good() != PWR_GOOD_OK) && (ESP::millis() - timer) < 250);

  if ((ESP::millis() - timer) >= 250) {
    mcp.WAKEUP_CLEAR();
    mcp.VCOM_CLEAR();
    mcp.PWRUP_CLEAR();
    return;
  }

  mcp.OE_SET();
  set_panel_state(ON);
}

uint8_t EInk::read_power_good()
{
  wire.begin_transmission(EINK_ADDRESS);
  wire.write(0x0F);
  wire.end_transmission();

  wire.request_from(EINK_ADDRESS, 1);
  return wire.read();
}

// LOW LEVEL FUNCTIONS

void EInk::vscan_start()
{
  CKV_SET;    ESP::delay_microseconds( 7);
  mcp.SPV_CLEAR();  ESP::delay_microseconds(10);
  CKV_CLEAR;  ESP::delay_microseconds( 0);
  CKV_SET;    ESP::delay_microseconds( 8);
  mcp.SPV_SET();    ESP::delay_microseconds(10);
  CKV_CLEAR;  ESP::delay_microseconds( 0);
  CKV_SET;    ESP::delay_microseconds(18);
  CKV_CLEAR;  ESP::delay_microseconds( 0);
  CKV_SET;    ESP::delay_microseconds(18);
  CKV_CLEAR;  ESP::delay_microseconds( 0);
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
  ESP::delay_microseconds(0);
}

void EInk::pins_z_state()
{
  gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_32, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_33, GPIO_MODE_INPUT);

  mcp.set_direction(MCP::OE,   MCP::INPUT);
  mcp.set_direction(MCP::GMOD, MCP::INPUT);
  mcp.set_direction(MCP::SPV,  MCP::INPUT);

  gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_5, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_19, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_23, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_25, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_26, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_27, GPIO_MODE_INPUT);
}

void EInk::pins_as_outputs()
{
  gpio_set_direction(GPIO_NUM_0, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_33, GPIO_MODE_OUTPUT);

  mcp.set_direction(MCP::OE,   MCP::OUTPUT);
  mcp.set_direction(MCP::GMOD, MCP::OUTPUT);
  mcp.set_direction(MCP::SPV,  MCP::OUTPUT);

  gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_23, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_26, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
}