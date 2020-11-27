#define __INKPLATE6_CTRL__ 1
#include "inkplate6_ctrl.hpp"

#include "logging.hpp"

#include "wire.hpp"
#include "mcp.hpp"
#include "esp.hpp"
#include "eink.hpp"

#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_sleep.h"

InkPlate6Ctrl InkPlate6Ctrl::singleton;

bool 
InkPlate6Ctrl::sd_card_setup()
{
  ESP_LOGI(TAG, "Setup SD card");

  static const gpio_num_t PIN_NUM_MISO = GPIO_NUM_12;
  static const gpio_num_t PIN_NUM_MOSI = GPIO_NUM_13;
  static const gpio_num_t PIN_NUM_CLK  = GPIO_NUM_14;
  static const gpio_num_t PIN_NUM_CS   = GPIO_NUM_15;

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();

  slot_config.gpio_miso = PIN_NUM_MISO;
  slot_config.gpio_mosi = PIN_NUM_MOSI;
  slot_config.gpio_sck  = PIN_NUM_CLK;
  slot_config.gpio_cs   = PIN_NUM_CS;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };

  sdmmc_card_t* card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem.");
    } else {
      ESP_LOGE(TAG, "Failed to setup the SD card (%s).", esp_err_to_name(ret));
    }
    return false;
  }

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, card);

  return true;
}

bool
InkPlate6Ctrl::touchpad_int_setup()
{
  mcp.set_int_pin(MCP::TOUCH_0, MCP::RISING);
  mcp.set_int_pin(MCP::TOUCH_1, MCP::RISING);
  mcp.set_int_pin(MCP::TOUCH_2, MCP::RISING);

  mcp.set_int_output(MCP::INTPORTB, false, false, HIGH);

  return true;
}

bool
InkPlate6Ctrl::setup()
{
  wire.setup();

  // Setup the display
  if (!e_ink.setup()) return false;

  // Setup Touch pad
  if (!touchpad_int_setup()) return false;

  // Mount and check the SD Card
  if (!sd_card_setup()) return false;

  // Good to go
  return true;
}

uint8_t 
InkPlate6Ctrl::read_touchpad(uint8_t pad)
{
  return mcp.digital_read((MCP::Pin)((pad & 3) + 10));
}

uint8_t 
InkPlate6Ctrl::read_touchpads()
{
  uint16_t val = mcp.get_ports();
  return (val >> 10) & 7;
}

double 
InkPlate6Ctrl::read_battery()
{
  mcp.digital_write(MCP::BATTERY_SWITCH, HIGH);
  ESP::delay(1);
  
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);

  int16_t adc = ESP::analog_read(ADC1_CHANNEL_7); // ADC 1 Channel 7 is GPIO port 35
  
  mcp.digital_write(MCP::BATTERY_SWITCH, LOW);
  
  return (double(adc) * 3.3 * 2) / 4095.0;
}

/**
 * @brief Start light sleep
 * 
 * Will be waken up at the end of the time requested or when a key is pressed.
 * 
 * @param minutes_to_sleep Wait time in minutes
 * @return true The timer when through the end
 * @return false A key was pressed
 */
bool
InkPlate6Ctrl::light_sleep(uint32_t minutes_to_sleep)
{
  esp_err_t err;
  if ((err = esp_sleep_enable_timer_wakeup(minutes_to_sleep * 60e6)) != ESP_OK) {
    LOG_E("Unable to program Light Sleep wait time: %d", err);
  }
  else if ((err = esp_sleep_enable_ext0_wakeup(GPIO_NUM_34, 1)) != ESP_OK) {
    LOG_E("Unable to set ext0 WakeUp for Light Sleep: %d", err);
  } 
  else if ((err = esp_light_sleep_start()) != ESP_OK) {
    LOG_E("Unable to start Light Sleep mode: %d", err);
  }

  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}

void 
InkPlate6Ctrl::deep_sleep()
{
  esp_err_t err;
  if ((err = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER)) != ESP_OK) {
    LOG_E("Unable to disable Sleep wait time: %d", err);
  }
  if ((err = esp_sleep_enable_ext0_wakeup(GPIO_NUM_34, 1)) != ESP_OK) {
    LOG_E("Unable to set ext0 WakeUp for Deep Sleep: %d", err);
  }
  esp_deep_sleep_start();
}