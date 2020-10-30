#define __WIRE__ 1
#include "wire.hpp"

#include "logging.hpp"

#include "driver/i2c.h"

static const char * TAG = "Wire";

Wire Wire::singleton;

esp_err_t    
Wire::initialize()
{
  ESP_LOGD(TAG, "Initializing...");
  esp_err_t result = ESP_OK;

  if (!initialized) {
    i2c_config_t config;

    config.mode             = I2C_MODE_MASTER;
    config.scl_io_num       = 22;
    config.scl_pullup_en    = GPIO_PULLUP_DISABLE;
    config.sda_io_num       = 21;
    config.sda_pullup_en    = GPIO_PULLUP_DISABLE;
    config.master.clk_speed = 20000; // to be confirm

    result = i2c_param_config(I2C_NUM_0, &config);

    if (result == ESP_OK) {
      ESP_LOGD(TAG, "i2c_param_config Ok");
      result = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    }
    if (result == ESP_OK) {
      ESP_LOGD(TAG, "i2c_driver_install Ok");
    }
    else {
      ESP_LOGE(TAG, "Unablem to complete I2C config sequence: %s.", esp_err_to_name(result));
    }
    initialized = result == ESP_OK;
  }

  return result;
}

esp_err_t    
Wire::begin_transmission(uint8_t addr)
{
  if (!initialized) initialize();

  if (initialized) {
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, 1);

    return ESP_OK;
  }
  else {
    return ESP_ERR_INVALID_STATE;
  }
}

esp_err_t     
Wire::end_transmission()
{
  esp_err_t result;

  if (initialized) {
    if ((result = i2c_master_write(cmd, buffer, index, true)) != ESP_OK) {
      ESP_LOGE(TAG, "Unable to write data to I2C: %s.", esp_err_to_name(result));
    }
    if ((result = i2c_master_stop(cmd)) != ESP_OK) {
      ESP_LOGE(TAG, "Unable to stop I2C: %s.", esp_err_to_name(result));
    }
    esp_err_t result = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    index = 0;
    if (result != ESP_OK) {
      ESP_LOGE(TAG, "Unable to complete Wire Transmission: %s.", esp_err_to_name(result));
    }
    return result;
  }
  else {
    return ESP_ERR_INVALID_STATE;
  }
}

esp_err_t    
Wire::write(uint8_t val)
{
  if (initialized) {
    buffer[index++] = val;
    if (index >= BUFFER_LENGTH) index = BUFFER_LENGTH - 1;
    return ESP_OK;
  }
  else {
    return ESP_ERR_INVALID_STATE;
  }
}

uint8_t 
Wire::read()
{
  if (!initialized || (index >= size_to_read)) return 0;
  return buffer[index++];
}

esp_err_t    
Wire::request_from(uint8_t addr, uint8_t size)
{
  if (!initialized) initialize();

  if (initialized) {
    if (size == 0) return ESP_OK;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);

    if (size > 1) i2c_master_read(cmd, buffer, size - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, buffer + size - 1, I2C_MASTER_NACK);

    i2c_master_stop(cmd);
    esp_err_t result = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    size_to_read = size;
    index = 0;
    if (result != ESP_OK) {
      ESP_LOGE(TAG, "Unable to complete request_from: %s.", esp_err_to_name(result));
    }
    return result;
  }
  else {
    return ESP_ERR_INVALID_STATE;
  }
}

