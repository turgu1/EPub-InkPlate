/*
eink.cpp
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

#define __EINK__ 1
#include "eink.hpp"

#include "logging.hpp"

#include "wire.hpp"
#include "mcp.hpp"
#include "esp.hpp"

#include <iostream>

EInk EInk::singleton;

const uint8_t EInk::WAVEFORM_3BIT[8][8] = {
  {0, 0, 0, 0, 1, 1, 1, 0}, {1, 2, 2, 2, 1, 1, 1, 0}, {0, 1, 2, 1, 1, 2, 1, 0},
  {0, 2, 1, 2, 1, 2, 1, 0}, {0, 0, 0, 1, 1, 1, 2, 0}, {2, 1, 1, 1, 2, 1, 2, 0},
  {1, 1, 1, 2, 1, 2, 2, 0}, {0, 0, 0, 0, 0, 0, 2, 0} };

const uint32_t EInk::WAVEFORM[50] = {
    0x00000008, 0x00000008, 0x00200408, 0x80281888, 0x60A81898, 0x60A8A8A8, 0x60A8A8A8, 0x6068A868, 0x6868A868,
    0x6868A868, 0x68686868, 0x6A686868, 0x5A686868, 0x5A686868, 0x5A586A68, 0x5A5A6A68, 0x5A5A6A68, 0x55566A68,
    0x55565A64, 0x55555654, 0x55555556, 0x55555556, 0x55555556, 0x55555516, 0x55555596, 0x15555595, 0x95955595,
    0x95959595, 0x95949495, 0x94949495, 0x94949495, 0xA4949494, 0x9494A4A4, 0x84A49494, 0x84948484, 0x84848484,
    0x84848484, 0x84848484, 0xA5A48484, 0xA9A4A4A8, 0xA9A8A8A8, 0xA5A9A9A4, 0xA5A5A5A4, 0xA1A5A5A1, 0xA9A9A9A9,
    0xA9A9A9A9, 0xA9A9A9A9, 0xA9A9A9A9, 0x15151515, 0x11111111 };

const uint8_t EInk::LUT2[16] = {
  0xAA, 0xA9, 0xA6, 0xA5, 0x9A, 0x99, 0x96, 0x95,
  0x6A, 0x69, 0x66, 0x65, 0x5A, 0x59, 0x56, 0x55 };

const uint8_t EInk::LUTW[16] = {
  0xFF, 0xFE, 0xFB, 0xFA, 0xEF, 0xEE, 0xEB, 0xEA,
  0xBF, 0xBE, 0xBB, 0xBA, 0xAF, 0xAE, 0xAB, 0xAA };

const uint8_t EInk::LUTB[16] = {
  0xFF, 0xFD, 0xF7, 0xF5, 0xDF, 0xDD, 0xD7, 0xD5,
  0x7F, 0x7D, 0x77, 0x75, 0x5F, 0x5D, 0x57, 0x55 };

// PIN_LUT built from the following:
//
// for (uint32_t i = 0; i < 256; i++) {
//   PIN_LUT[i] =  ((i & 0b00000011) << 4)        | 
//                (((i & 0b00001100) >> 2) << 18) | 
//                (((i & 0b00010000) >> 4) << 23) |
//                (((i & 0b11100000) >> 5) << 25);
// }

const uint32_t EInk::PIN_LUT[256] = {
  0x00000000, 0x00000010, 0x00000020, 0x00000030, 0x00040000, 0x00040010, 0x00040020, 0x00040030, 
  0x00080000, 0x00080010, 0x00080020, 0x00080030, 0x000c0000, 0x000c0010, 0x000c0020, 0x000c0030, 
  0x00800000, 0x00800010, 0x00800020, 0x00800030, 0x00840000, 0x00840010, 0x00840020, 0x00840030, 
  0x00880000, 0x00880010, 0x00880020, 0x00880030, 0x008c0000, 0x008c0010, 0x008c0020, 0x008c0030, 
  0x02000000, 0x02000010, 0x02000020, 0x02000030, 0x02040000, 0x02040010, 0x02040020, 0x02040030, 
  0x02080000, 0x02080010, 0x02080020, 0x02080030, 0x020c0000, 0x020c0010, 0x020c0020, 0x020c0030, 
  0x02800000, 0x02800010, 0x02800020, 0x02800030, 0x02840000, 0x02840010, 0x02840020, 0x02840030, 
  0x02880000, 0x02880010, 0x02880020, 0x02880030, 0x028c0000, 0x028c0010, 0x028c0020, 0x028c0030, 
  0x04000000, 0x04000010, 0x04000020, 0x04000030, 0x04040000, 0x04040010, 0x04040020, 0x04040030, 
  0x04080000, 0x04080010, 0x04080020, 0x04080030, 0x040c0000, 0x040c0010, 0x040c0020, 0x040c0030, 
  0x04800000, 0x04800010, 0x04800020, 0x04800030, 0x04840000, 0x04840010, 0x04840020, 0x04840030, 
  0x04880000, 0x04880010, 0x04880020, 0x04880030, 0x048c0000, 0x048c0010, 0x048c0020, 0x048c0030, 
  0x06000000, 0x06000010, 0x06000020, 0x06000030, 0x06040000, 0x06040010, 0x06040020, 0x06040030, 
  0x06080000, 0x06080010, 0x06080020, 0x06080030, 0x060c0000, 0x060c0010, 0x060c0020, 0x060c0030, 
  0x06800000, 0x06800010, 0x06800020, 0x06800030, 0x06840000, 0x06840010, 0x06840020, 0x06840030, 
  0x06880000, 0x06880010, 0x06880020, 0x06880030, 0x068c0000, 0x068c0010, 0x068c0020, 0x068c0030, 
  0x08000000, 0x08000010, 0x08000020, 0x08000030, 0x08040000, 0x08040010, 0x08040020, 0x08040030, 
  0x08080000, 0x08080010, 0x08080020, 0x08080030, 0x080c0000, 0x080c0010, 0x080c0020, 0x080c0030, 
  0x08800000, 0x08800010, 0x08800020, 0x08800030, 0x08840000, 0x08840010, 0x08840020, 0x08840030, 
  0x08880000, 0x08880010, 0x08880020, 0x08880030, 0x088c0000, 0x088c0010, 0x088c0020, 0x088c0030, 
  0x0a000000, 0x0a000010, 0x0a000020, 0x0a000030, 0x0a040000, 0x0a040010, 0x0a040020, 0x0a040030, 
  0x0a080000, 0x0a080010, 0x0a080020, 0x0a080030, 0x0a0c0000, 0x0a0c0010, 0x0a0c0020, 0x0a0c0030, 
  0x0a800000, 0x0a800010, 0x0a800020, 0x0a800030, 0x0a840000, 0x0a840010, 0x0a840020, 0x0a840030, 
  0x0a880000, 0x0a880010, 0x0a880020, 0x0a880030, 0x0a8c0000, 0x0a8c0010, 0x0a8c0020, 0x0a8c0030, 
  0x0c000000, 0x0c000010, 0x0c000020, 0x0c000030, 0x0c040000, 0x0c040010, 0x0c040020, 0x0c040030, 
  0x0c080000, 0x0c080010, 0x0c080020, 0x0c080030, 0x0c0c0000, 0x0c0c0010, 0x0c0c0020, 0x0c0c0030, 
  0x0c800000, 0x0c800010, 0x0c800020, 0x0c800030, 0x0c840000, 0x0c840010, 0x0c840020, 0x0c840030, 
  0x0c880000, 0x0c880010, 0x0c880020, 0x0c880030, 0x0c8c0000, 0x0c8c0010, 0x0c8c0020, 0x0c8c0030, 
  0x0e000000, 0x0e000010, 0x0e000020, 0x0e000030, 0x0e040000, 0x0e040010, 0x0e040020, 0x0e040030, 
  0x0e080000, 0x0e080010, 0x0e080020, 0x0e080030, 0x0e0c0000, 0x0e0c0010, 0x0e0c0020, 0x0e0c0030, 
  0x0e800000, 0x0e800010, 0x0e800020, 0x0e800030, 0x0e840000, 0x0e840010, 0x0e840020, 0x0e840030, 
  0x0e880000, 0x0e880010, 0x0e880020, 0x0e880030, 0x0e8c0000, 0x0e8c0010, 0x0e8c0020, 0x0e8c0030
};

bool 
EInk::setup()
{
  ESP_LOGD(TAG, "Initializing...");

  if (initialized) return true;

  wire.setup();
  
  if (!mcp.setup()) {
    ESP_LOGE(TAG, "Initialization not completed (MCP Issue).");
    return false;
  }
  else {
    ESP_LOGD(TAG, "MCP initialized.");
  }

  mcp.set_direction(MCP::VCOM,         MCP::OUTPUT);
  mcp.set_direction(MCP::PWRUP,        MCP::OUTPUT);
  mcp.set_direction(MCP::WAKEUP,       MCP::OUTPUT); 
  mcp.set_direction(MCP::GPIO0_ENABLE, MCP::OUTPUT);
  mcp.digital_write(MCP::GPIO0_ENABLE, HIGH);

  mcp.wakeup_set(); 
 
  //ESP_LOGD(TAG, "Power Mgr Init..."); fflush(stdout);

  ESP::delay_microseconds(1800);
  wire.begin_transmission(PWRMGR_ADDRESS);
  wire.write(0x09);
  wire.write(0b00011011); // Power up seq.
  wire.write(0b00000000); // Power up delay (3mS per rail)
  wire.write(0b00011011); // Power down seq.
  wire.write(0b00000000); // Power down delay (6mS per rail)
  wire.end_transmission();
  ESP::delay(1);

  //ESP_LOGD(TAG, "Power init completed");

  mcp.wakeup_clear();

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

  d_memory_new = (Bitmap1Bit *) ESP::ps_malloc(BITMAP_SIZE_1BIT);
  p_buffer     = (uint8_t *)    ESP::ps_malloc(120000);

  ESP_LOGD(TAG, "Memory allocation for bitmap buffers.");
  ESP_LOGD(TAG, "d_memory_new: %08x p_buffer: %08x.", (unsigned int)d_memory_new, (unsigned int)p_buffer);

  if ((d_memory_new == nullptr) || 
      (p_buffer     == nullptr)) {
    do {
      ESP_LOGE(TAG, "Unable to complete buffers allocation");
      ESP::delay(10000);
    } while (true);
  }

  memset(d_memory_new, 0, BITMAP_SIZE_1BIT);
  memset(p_buffer,     0, 120000);

  initialized = true;
  return true;
}

void
EInk::update_1bit(const Bitmap1Bit & bitmap)
{
  ESP_LOGD(TAG, "update_1bit...");
 
  const uint8_t * ptr;
  uint32_t        send;
  uint8_t         dram;

  turn_on();

  clean_fast(0,  1);

  for (int8_t k = 0; k < 4; k++) {
    ptr = &bitmap[BITMAP_SIZE_1BIT - 1];
    vscan_start();

    for (uint16_t i = 0; i < HEIGHT; i++) {
      dram = *ptr--;
      send = PIN_LUT[LUTB[dram >> 4]];
      hscan_start(send);
      send = PIN_LUT[LUTB[dram & 0x0F]];
      GPIO.out_w1ts = CL | send;
      GPIO.out_w1tc = CL | DATA;

      for (uint16_t j = 0; j < LINE_SIZE_1BIT - 1; j++) {
        dram = *ptr--;
        send = PIN_LUT[LUTB[dram >> 4]];
        GPIO.out_w1ts = CL | send;
        GPIO.out_w1tc = CL | DATA;
        send = PIN_LUT[LUTB[dram & 0x0F]];
        GPIO.out_w1ts = CL | send;
        GPIO.out_w1tc = CL | DATA;
      }

      GPIO.out_w1ts = CL | send;
      GPIO.out_w1tc = CL | DATA;
      vscan_end();
    }
    ESP::delay_microseconds(230);
  }

  ptr = &bitmap[BITMAP_SIZE_1BIT - 1];
  vscan_start();
 
  for (uint16_t i = 0; i < HEIGHT; i++) {
    dram = *ptr--;
    send = PIN_LUT[LUT2[dram >> 4]];
    hscan_start(send);
    send = PIN_LUT[LUT2[dram & 0x0F]];
    GPIO.out_w1ts = CL | send;
    GPIO.out_w1tc = CL | DATA;
    
    for (uint16_t j = 0; j < LINE_SIZE_1BIT - 1; j++) {
      dram = *ptr--;
      send = PIN_LUT[LUT2[dram >> 4]];
      GPIO.out_w1ts = CL | send;
      GPIO.out_w1tc = CL | DATA;
      send = PIN_LUT[LUT2[dram & 0x0F]];
      GPIO.out_w1ts = CL | send;
      GPIO.out_w1tc = CL | DATA;
    }

    GPIO.out_w1ts = CL | send;
    GPIO.out_w1tc = CL | DATA;
    vscan_end();
  }
  ESP::delay_microseconds(230);

  vscan_start();

  send = PIN_LUT[0];
  for (uint16_t i = 0; i < HEIGHT; i++) {
    hscan_start(send);
    GPIO.out_w1ts = CL | send;
    GPIO.out_w1tc = CL | DATA;

    for (int j = 0; j < LINE_SIZE_1BIT - 1; j++) {
      GPIO.out_w1ts = CL | send;
      GPIO.out_w1tc = CL | DATA;
      GPIO.out_w1ts = CL | send;
      GPIO.out_w1tc = CL | DATA;
    }

    GPIO.out_w1ts = CL | send;
    GPIO.out_w1tc = CL | DATA;
    vscan_end();
  }

  ESP::delay_microseconds(230);

  vscan_start();
  turn_off();
  
  memcpy(d_memory_new, &bitmap, BITMAP_SIZE_1BIT);
  partial_allowed = true;
}

void
EInk::update_3bit(const Bitmap3Bit & bitmap)
{
  ESP_LOGD(TAG, "Update_3bit...");

  turn_on();

  clean_fast(0,  1);
  clean_fast(1, 21);
  clean_fast(2,  1);
  clean_fast(0, 12);
  clean_fast(2,  1);
  clean_fast(1, 21);
  clean_fast(2,  1);
  clean_fast(0, 12);

  for (int k = 0; k < 8; k++) {

    const uint8_t * dp = &bitmap[BITMAP_SIZE_3BIT - 1];
    uint32_t send;
    uint8_t  pix1;
    uint8_t  pix2;
    uint8_t  pix3;
    uint8_t  pix4;
    uint8_t  pixel;
    uint8_t  pixel2;

    vscan_start();

    for (int i = 0; i < HEIGHT; i++) {
      pixel  = 0;
      pixel2 = 0;
      pix1   = *(dp--);
      pix2   = *(dp--);
      pix3   = *(dp--);
      pix4   = *(dp--);

      pixel  |= (WAVEFORM_3BIT[ pix1       & 0x07][k] << 6) | 
                (WAVEFORM_3BIT[(pix1 >> 4) & 0x07][k] << 4) |
                (WAVEFORM_3BIT[ pix2       & 0x07][k] << 2) | 
                (WAVEFORM_3BIT[(pix2 >> 4) & 0x07][k] << 0);

      pixel2 |= (WAVEFORM_3BIT[ pix3       & 0x07][k] << 6) | 
                (WAVEFORM_3BIT[(pix3 >> 4) & 0x07][k] << 4) |
                (WAVEFORM_3BIT[ pix4       & 0x07][k] << 2) |
                (WAVEFORM_3BIT[(pix4 >> 4) & 0x07][k] << 0);

      send = PIN_LUT[pixel];
      hscan_start(send);
      send = PIN_LUT[pixel2];
      GPIO.out_w1ts = CL | send;
      GPIO.out_w1tc = CL | DATA;

      for (int j = 0; j < (LINE_SIZE_3BIT >> 2) - 1; j++) {
        pixel  = 0;
        pixel2 = 0;
        pix1   = *(dp--);
        pix2   = *(dp--);
        pix3   = *(dp--);
        pix4   = *(dp--);

        pixel  |= (WAVEFORM_3BIT[ pix1       & 0x07][k] << 6) | 
                  (WAVEFORM_3BIT[(pix1 >> 4) & 0x07][k] << 4) |
                  (WAVEFORM_3BIT[ pix2       & 0x07][k] << 2) | 
                  (WAVEFORM_3BIT[(pix2 >> 4) & 0x07][k] << 0);

        pixel2 |= (WAVEFORM_3BIT[ pix3       & 0x07][k] << 6) | 
                  (WAVEFORM_3BIT[(pix3 >> 4) & 0x07][k] << 4) |
                  (WAVEFORM_3BIT[ pix4       & 0x07][k] << 2) | 
                  (WAVEFORM_3BIT[(pix4 >> 4) & 0x07][k] << 0);

        send = PIN_LUT[pixel];
        GPIO.out_w1ts = CL | send;
        GPIO.out_w1tc = CL | DATA;

        send = PIN_LUT[pixel2];
        GPIO.out_w1ts = CL | send;
        GPIO.out_w1tc = CL | DATA;
      }

      GPIO.out_w1ts = CL | send;
      GPIO.out_w1tc = CL | DATA;
      vscan_end();
    }

    ESP::delay_microseconds(230);
  }

  clean_fast(3, 1);
  vscan_start();
  turn_off();
}

void
EInk::partial_update(const Bitmap1Bit & bitmap)
{
  if (!partial_allowed) {
    update_1bit(bitmap);
    return;
  }

  ESP_LOGD(TAG, "Partial update...");

  uint32_t send;
  uint32_t n   = 119999;
  uint16_t pos = BITMAP_SIZE_1BIT - 1;
  uint8_t  diffw, diffb;

  for (int i = 0; i < HEIGHT; i++) {
    for (int j = 0; j < LINE_SIZE_1BIT; j++) {
      diffw =  (*d_memory_new)[pos] & ~bitmap[pos];
      diffb = ~(*d_memory_new)[pos] &  bitmap[pos];
      pos--;
      p_buffer[n--] = LUTW[diffw >>   4] & (LUTB[diffb >>   4]);
      p_buffer[n--] = LUTW[diffw & 0x0F] & (LUTB[diffb & 0x0F]);
    }
  }

  turn_on();

  for (int k = 0; k < 5; k++) {
    vscan_start();
    n = 119999;

    for (int i = 0; i < HEIGHT; i++) {
      send = PIN_LUT[p_buffer[n--]];
      hscan_start(send);

      for (int j = 0; j < 199; j++) {
        send = PIN_LUT[p_buffer[n--]];
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;
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

  memcpy(d_memory_new, &bitmap, BITMAP_SIZE_1BIT);
}

void 
EInk::clean()
{
  ESP_LOGD(TAG, "Clean...");

  turn_on();

  int m = 0;
  clean_fast(0, 1);
  m++; clean_fast((WAVEFORM[m] >> 30) & 3,  8);
  m++; clean_fast((WAVEFORM[m] >> 24) & 3,  1);
  m++; clean_fast( WAVEFORM[m]        & 3,  8);
  m++; clean_fast((WAVEFORM[m] >>  6) & 3,  1);
  m++; clean_fast((WAVEFORM[m] >> 30) & 3, 10);
}

void
EInk::clean_fast(uint8_t c, uint8_t rep)
{
  static uint8_t byte[4] = { 0b10101010, 0b01010101, 0b00000000, 0b11111111 };

  turn_on();

  uint32_t send = PIN_LUT[byte[c]];

  for (int8_t k = 0; k < rep; k++) {

    vscan_start();

    for (uint16_t i = 0; i < HEIGHT; i++) {

      hscan_start(send);

      GPIO.out_w1ts = CL | send;
      GPIO.out_w1tc = CL;

      for (uint16_t j = 0; j < LINE_SIZE_1BIT - 1; j++) {
        GPIO.out_w1ts = CL;
        GPIO.out_w1tc = CL;
        GPIO.out_w1ts = CL;
        GPIO.out_w1tc = CL;
      }
      GPIO.out_w1ts = CL;
      GPIO.out_w1tc = CL;

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
 
      mcp.oe_clear();
    mcp.gmod_clear();

  GPIO.out &= ~(DATA | LE | CL);
  
         ckv_clear();
         sph_clear();
     mcp.spv_clear();
    mcp.vcom_clear();

        ESP::delay(6);

   mcp.pwrup_clear();
  mcp.wakeup_clear();

  unsigned long timer = ESP::millis();

  do {
    ESP::delay(1);
  } while ((read_power_good() != 0) && (ESP::millis() - timer) < 250);

  pins_z_state();
  set_panel_state(OFF);
}

// Turn on supply for epaper display (TPS65186) 
// [+15 VDC, -15VDC, +22VDC, -20VDC, +3.3VDC, VCOM]
void 
EInk::turn_on()
{
  if (get_panel_state() == ON) return;

  mcp.wakeup_set();
  ESP::delay_microseconds(1800);
  mcp.pwrup_set();

  // Enable all rails
  wire.begin_transmission(PWRMGR_ADDRESS);
  wire.write(0x01);
  wire.write(0b00111111);
  wire.end_transmission();

  pins_as_outputs();

      le_clear();
  mcp.oe_clear();
      cl_clear();
       sph_set();
  mcp.gmod_set();
   mcp.spv_set();
     ckv_clear();
  mcp.oe_clear();
  mcp.vcom_set();

  unsigned long timer = ESP::millis();

  do {
    ESP::delay(1);
  } while ((read_power_good() != PWR_GOOD_OK) && (ESP::millis() - timer) < 250);

  if ((ESP::millis() - timer) >= 250) {
    mcp.wakeup_clear();
      mcp.vcom_clear();
     mcp.pwrup_clear();
    return;
  }

  mcp.oe_set();
  set_panel_state(ON);
}

uint8_t 
EInk::read_power_good()
{
  wire.begin_transmission(PWRMGR_ADDRESS);
  wire.write(0x0F);
  wire.end_transmission();

  wire.request_from(PWRMGR_ADDRESS, 1);
  return wire.read();
}

// LOW LEVEL FUNCTIONS

void 
EInk::vscan_start()
{
        ckv_set(); ESP::delay_microseconds( 7);
  mcp.spv_clear(); ESP::delay_microseconds(10);
      ckv_clear(); ESP::delay_microseconds( 0);
        ckv_set(); ESP::delay_microseconds( 8);
    mcp.spv_set(); ESP::delay_microseconds(10);
      ckv_clear(); ESP::delay_microseconds( 0);
        ckv_set(); ESP::delay_microseconds(18);
      ckv_clear(); ESP::delay_microseconds( 0);
        ckv_set(); ESP::delay_microseconds(18);
      ckv_clear(); ESP::delay_microseconds( 0);
        ckv_set();
}

void 
EInk::hscan_start(uint32_t d)
{
  sph_clear();
  GPIO.out_w1ts = CL | d   ;
  GPIO.out_w1tc = CL | DATA;
  sph_set();
  ckv_set();
}

void 
EInk::vscan_end()
{
  ckv_clear();
     le_set();
   le_clear();

  ESP::delay_microseconds(0);
}

void 
EInk::pins_z_state()
{
  gpio_set_direction(GPIO_NUM_0,  GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_2,  GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_32, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_33, GPIO_MODE_INPUT);

  mcp.set_direction(MCP::OE,   MCP::INPUT);
  mcp.set_direction(MCP::GMOD, MCP::INPUT);
  mcp.set_direction(MCP::SPV,  MCP::INPUT);

  gpio_set_direction(GPIO_NUM_4,  GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_5,  GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_19, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_23, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_25, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_26, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_27, GPIO_MODE_INPUT);
}

void 
EInk::pins_as_outputs()
{
  gpio_set_direction(GPIO_NUM_0,  GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_2,  GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_33, GPIO_MODE_OUTPUT);

  mcp.set_direction(MCP::OE,   MCP::OUTPUT);
  mcp.set_direction(MCP::GMOD, MCP::OUTPUT);
  mcp.set_direction(MCP::SPV,  MCP::OUTPUT);

  gpio_set_direction(GPIO_NUM_4,  GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_5,  GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_23, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_26, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
}

int8_t 
EInk::read_temperature()
{
  int8_t temp;
    
  if (get_panel_state() == OFF) {
    mcp.wakeup_set();
    ESP::delay_microseconds(1800);
     mcp.pwrup_set();

    ESP::delay(5);
  }

  wire.begin_transmission(PWRMGR_ADDRESS);
  wire.write(0x0D);
  wire.write(0b10000000);
  wire.end_transmission();
    
  ESP::delay(5);

  wire.begin_transmission(PWRMGR_ADDRESS);
  wire.write(0x00);
  wire.end_transmission();

  wire.request_from(PWRMGR_ADDRESS, 1);
  temp = wire.read();
    
  if (get_panel_state() == OFF) {
    mcp.pwrup_clear();
    mcp.wakeup_clear();
    ESP::delay(5);
  }

  return temp;
}

