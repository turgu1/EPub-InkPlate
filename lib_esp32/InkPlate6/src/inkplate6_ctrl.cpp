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
InkPlate6Ctrl::setup()
{
  // Mount and check the SD Card
  if (!sd_card_setup()) return false;

  // Setup the display
  if (!e_ink.setup()) return false;

  // Good to go
  return true;
}

uint8_t 
InkPlate6Ctrl::read_touchpad(uint8_t pad)
{
  return mcp.digital_read((MCP::Pin)((pad & 3) + 10));
}

double 
InkPlate6Ctrl::read_battery()
{
  mcp.digital_write(MCP::BATTERY_SWITCH, HIGH);
  ESP::delay(1);
  int16_t adc = ESP::analog_read(ADC1_CHANNEL_7); // ADC 1 Channel 7 is GPIO port 35
  mcp.digital_write(MCP::BATTERY_SWITCH, LOW);

  ESP_LOGE(TAG, "Battery adc readout: %d.", adc);
  
  return ((double(adc) / 4095.0) * 3.3 * 2);
}
