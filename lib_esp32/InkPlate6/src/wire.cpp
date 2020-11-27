
// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __WIRE__ 1
#include "wire.hpp"

#include "logging.hpp"
#include "driver/i2c.h"

#include <cstring>

Wire Wire::singleton;

void
Wire::setup()
{
  ESP_LOGD(TAG, "Initializing...");

  if (!initialized) {
    i2c_config_t config;

    memset(&config, 0, sizeof(i2c_config_t));

    config.mode             = I2C_MODE_MASTER;
    config.scl_io_num       = GPIO_NUM_22;
    config.scl_pullup_en    = GPIO_PULLUP_DISABLE;
    config.sda_io_num       = GPIO_NUM_21;
    config.sda_pullup_en    = GPIO_PULLUP_DISABLE;
    config.master.clk_speed = 1E6;

    ESP_ERROR_CHECK(  i2c_param_config(I2C_NUM_0, &config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    initialized = true; 
  }
}

void   
Wire::begin_transmission(uint8_t addr)
{
  if (!initialized) setup();

  if (initialized) {
    address = addr;
    index   = 0;
  }
}

void     
Wire::end_transmission()
{
  if (initialized) {
    //ESP_LOGD(TAG, "Writing %d bytes to i2c at address 0x%02x.", index, address);

    // for (int i = 0; i < index; i++) {
    //   printf("%02x ", buffer[i]);
    // }
    // printf("\n");
    // fflush(stdout);

    cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, 1));
    if (index > 0) ESP_ERROR_CHECK(i2c_master_write(cmd, buffer, index, 1));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS));
    i2c_cmd_link_delete(cmd);
    index = 0;
  }
  
  // ESP_LOGD(TAG, "I2C Transmission completed.");
}

void    
Wire::write(uint8_t val)
{
  if (initialized) {    
    buffer[index++] = val;
    if (index >= BUFFER_LENGTH) index = BUFFER_LENGTH - 1;
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
  if (!initialized) setup();

  if (initialized) {
    if (size == 0) return ESP_OK;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true));

    if (size > 1) ESP_ERROR_CHECK(i2c_master_read(cmd, buffer, size - 1, I2C_MASTER_ACK));
    ESP_ERROR_CHECK(i2c_master_read_byte(cmd, buffer + size - 1, I2C_MASTER_LAST_NACK));

    ESP_ERROR_CHECK(i2c_master_stop(cmd));

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

