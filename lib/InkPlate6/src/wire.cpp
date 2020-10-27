#define __WIRE__ 1
#include "wire.hpp"

#include "driver/i2c.h"

Wire Wire::singleton;

esp_err_t    
Wire::begin()
{
  esp_err_t result = ESP_OK;

  if (!initialized) {
    i2c_config_t config = {
      .mode             = I2C_MODE_MASTER,
      .scl_io_num       = 22,
      .scl_pullup_en    = GPIO_PULLUP_DISABLE,
      .sda_io_num       = 21,
      .sda_pullup_en    = GPIO_PULLUP_DISABLE,
      .master.clk_speed = 100000
    };

    result = i2c_param_config(I2C_NUM_0, &config);

    if (result == ESP_OK) {
      result = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    }

    initialized = result == ESP_OK;
  }

  return result;
}

void    
Wire::begin_transmission(uint8_t addr)
{
  if (!initialized) begin();
}

int     
Wire::end_transmission()
{
  if (!initialized) begin();

}

void    
Wire::write(uint8_t val)
{
  if (!initialized) begin();

}

uint8_t 
Wire::read()
{
  if (!initialized) begin();

}

void    
Wire::request_from(uint8_t addr, uint8_t size)
{
  if (!initialized) begin();

}

