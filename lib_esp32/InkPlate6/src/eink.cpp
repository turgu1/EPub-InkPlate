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

#include "logging.hpp"

#include "wire.hpp"
#include "mcp.hpp"
#include "esp.hpp"

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

EInk::EInk() :
  panel_state(OFF), 
  initialized(false),
  partial_allowed(false)  
{
  for (uint32_t i = 0; i < 256; ++i) {
    pin_lut[i] =  ((i & 0b00000011) << 4)        | 
                 (((i & 0b00001100) >> 2) << 18) | 
                 (((i & 0b00010000) >> 4) << 23) |
                 (((i & 0b11100000) >> 5) << 25);
  }
}

// void EInk::test()
// {
//   printf("Wakeup set...\n"); fflush(stdout);

//   if (!mcp.initialize()) {
//     ESP_LOGE(TAG, "Initialization not completed (MCP Issue).");
//     return;
//   }
//   else {
//     ESP_LOGD(TAG, "MCP initialized.");
//   }

//   mcp.set_direction(MCP::WAKEUP, MCP::OUTPUT);

//   mcp.wakeup_set();

//   // mcp.test();

//   ESP::delay(2000);

//   ESP_LOGD(TAG, "Power Mgr...");
  
//   wire.begin_transmission(PWRMGR_ADDRESS);
//   wire.end_transmission();

//   mcp.wakeup_clear();
// }

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

  mcp.set_direction(MCP::WAKEUP, MCP::OUTPUT); ESP::delay(100);
  mcp.wakeup_set();                            ESP::delay(100);

  ESP_LOGD(TAG, "Power Mgr Init..."); fflush(stdout);

  wire.begin_transmission(PWRMGR_ADDRESS);
  wire.write(0x09);
  wire.write(0b00011011); // Power up seq.
  wire.write(0b00000000); // Power up delay (3mS per rail)
  wire.write(0b00011011); // Power down seq.
  wire.write(0b00000000); // Power down delay (6mS per rail)
  wire.end_transmission();
  ESP::delay(1);

  ESP_LOGD(TAG, "Power init completed");

  mcp.wakeup_clear();

  mcp.set_direction(MCP::VCOM,         MCP::OUTPUT);
  mcp.set_direction(MCP::PWRUP,        MCP::OUTPUT);
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
  memcpy(d_memory_new, &bitmap, BITMAP_SIZE_1BIT);

  uint32_t send;
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
    const uint8_t * ptr = &bitmap[BITMAP_SIZE_1BIT - 1];
    vscan_start();

    for (int i = 0; i < HEIGHT; ++i) {
      dram = *ptr--;
      send = pin_lut[LUTB[dram >> 4]];
      hscan_start(send);
      send = pin_lut[LUTB[dram & 0x0F]];
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;

      for (int j = 0; j < LINE_SIZE_1BIT - 1; ++j) {
        dram = *ptr--;
        send = pin_lut[LUTB[dram >> 4]];
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;
        send = pin_lut[LUTB[dram & 0x0F]];
        GPIO.out_w1ts = send | CL;
        GPIO.out_w1tc = DATA | CL;
      }

      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
      vscan_end();
    }
    ESP::delay_microseconds(230);
  }

  uint16_t pos = BITMAP_SIZE_1BIT - 1;
  vscan_start();

  for (int i = 0; i < HEIGHT; ++i) {
    dram = (*d_memory_new)[pos];
    send = pin_lut[LUT2[dram >> 4]];
    hscan_start(send);
    send = pin_lut[LUT2[dram & 0x0F]];
    GPIO.out_w1ts = send | CL;
    GPIO.out_w1tc = DATA | CL;
    pos--;
    
    for (int j = 0; j < LINE_SIZE_1BIT - 1; ++j) {
      dram = (*d_memory_new)[pos];
      send = pin_lut[LUT2[dram >> 4]];
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;
      send = pin_lut[LUT2[dram & 0x0F]];
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
    dram = (*d_memory_new)[pos];
    send = pin_lut[0];
    hscan_start(send);
    GPIO.out_w1ts = send | CL;
    GPIO.out_w1tc = DATA | CL;

    for (int j = 0; j < BITMAP_SIZE_1BIT - 1; ++j) {
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
  
  partial_allowed = false;
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

  for (int k = 0; k < 8; ++k) {
    const uint8_t * dp = &bitmap[BITMAP_SIZE_3BIT - 1];
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
      pixel  |= (WAVEFORM_3BIT[pix1 & 0x07][k] << 6) | (WAVEFORM_3BIT[(pix1 >> 4) & 0x07][k] << 4) |
                (WAVEFORM_3BIT[pix2 & 0x07][k] << 2) | (WAVEFORM_3BIT[(pix2 >> 4) & 0x07][k] << 0);
      pixel2 |= (WAVEFORM_3BIT[pix3 & 0x07][k] << 6) | (WAVEFORM_3BIT[(pix3 >> 4) & 0x07][k] << 4) |
                (WAVEFORM_3BIT[pix4 & 0x07][k] << 2) | (WAVEFORM_3BIT[(pix4 >> 4) & 0x07][k] << 0);

      send = pin_lut[pixel];
      hscan_start(send);
      send = pin_lut[pixel2];
      GPIO.out_w1ts = send | CL;
      GPIO.out_w1tc = DATA | CL;

      for (int j = 0; j < (LINE_SIZE_3BIT >> 2)- 1; ++j) {
        pixel  = 0;
        pixel2 = 0;
        pix1   = *(dp--);
        pix2   = *(dp--);
        pix3   = *(dp--);
        pix4   = *(dp--);

        pixel  |= (WAVEFORM_3BIT[pix1 & 0x07][k] << 6) | (WAVEFORM_3BIT[(pix1 >> 4) & 0x07][k] << 4) |
                  (WAVEFORM_3BIT[pix2 & 0x07][k] << 2) | (WAVEFORM_3BIT[(pix2 >> 4) & 0x07][k] << 0);
        pixel2 |= (WAVEFORM_3BIT[pix3 & 0x07][k] << 6) | (WAVEFORM_3BIT[(pix3 >> 4) & 0x07][k] << 4) |
                  (WAVEFORM_3BIT[pix4 & 0x07][k] << 2) | (WAVEFORM_3BIT[(pix4 >> 4) & 0x07][k] << 0);

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
  ESP::delay_microseconds(230);
  turn_off();
}

void 
EInk::partial_update(const Bitmap1Bit & bitmap)
{
  if (!partial_allowed) update_1bit(bitmap);

  ESP_LOGD(TAG, "Partial update...");

  uint32_t n    = 119999;
  uint32_t send;
  uint16_t pos  = BITMAP_SIZE_1BIT - 1;
  uint8_t  diffw, diffb;

  for (int i = 0; i < HEIGHT; ++i) {
    for (int j = 0; j < LINE_SIZE_1BIT; ++j) {
      diffw =  (*d_memory_new)[pos] & ~bitmap[pos];
      diffb = ~(*d_memory_new)[pos] &  bitmap[pos];
      pos--;
      p_buffer[n] = LUTW[diffw >> 4] & (LUTB[diffb >> 4]);
      n--;
      p_buffer[n] = LUTW[diffw & 0x0F] & (LUTB[diffb & 0x0F]);
      n--;
    }
  }

  turn_on();

  for (int k = 0; k < 5; ++k) {
    vscan_start();
    n = 119999;

    for (int i = 0; i < HEIGHT; ++i) {
      send = pin_lut[p_buffer[n]];
      hscan_start(send);
      n--;

      for (int j = 0; j < 199; ++j) {
        send = pin_lut[p_buffer[n]];
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
  //ESP_LOGD(TAG, "Clean Fast %d, %d", c, rep);

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

      for (int j = 0; j < LINE_SIZE_1BIT - 1; ++j) {
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

// Turn on supply for epaper display (TPS65186) [+15 VDC, -15VDC, +22VDC, -20VDC, +3.3VDC, VCOM]
void EInk::turn_on()
{
  if (get_panel_state() == ON) return;

  mcp.wakeup_set();
  ESP::delay(1);
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

uint8_t EInk::read_power_good()
{
  wire.begin_transmission(PWRMGR_ADDRESS);
  wire.write(0x0F);
  wire.end_transmission();

  wire.request_from(PWRMGR_ADDRESS, 1);
  return wire.read();
}

// LOW LEVEL FUNCTIONS

void EInk::vscan_start()
{
  ckv_set();         ESP::delay_microseconds( 7);
  mcp.spv_clear();   ESP::delay_microseconds(10);
  ckv_clear();       ESP::delay_microseconds( 0);
  ckv_set();         ESP::delay_microseconds( 8);
  mcp.spv_set();     ESP::delay_microseconds(10);
  ckv_clear();       ESP::delay_microseconds( 0);
  ckv_set();         ESP::delay_microseconds(18);
  ckv_clear();       ESP::delay_microseconds( 0);
  ckv_set();         ESP::delay_microseconds(18);
  ckv_clear();       ESP::delay_microseconds( 0);
  ckv_set();
}

void EInk::hscan_start(uint32_t d)
{
  sph_clear();
  GPIO.out_w1ts = d    | CL;
  GPIO.out_w1tc = DATA | CL;
  sph_set();
  ckv_set();
}

void EInk::vscan_end()
{
  ckv_clear();
  le_set();
  le_clear();
  ESP::delay_microseconds(0);
}

void EInk::pins_z_state()
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

void EInk::pins_as_outputs()
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

  wire.request_from(0x48, 1);
  temp = wire.read();
    
  if (get_panel_state() == OFF) {
    mcp.pwrup_clear();
    mcp.wakeup_clear();
    ESP::delay(5);
  }

  return temp;
}

