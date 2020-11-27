// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __ESP_HPP__
#define __ESP_HPP__

#include <cinttypes>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#include "logging.hpp"

static const uint8_t HIGH = 1;
static const uint8_t LOW  = 0;

/**
 * @brief ESP-IDF support methods
 * 
 * These are class methods that simplify access to some esp-idf specifics.
 * The class is taylored for the needs of the low-level driver classes of the InkPlate-6
 * software.
 */
class ESP
{
  private:
    static constexpr char const * TAG = "ESP";
    
  public:
    static inline long millis() { return (unsigned long) (esp_timer_get_time() / 1000); }

    static void IRAM_ATTR delay_microseconds(uint32_t micro_seconds) {
      uint64_t m = esp_timer_get_time();
      if (micro_seconds > 2) {
        uint64_t e = m + micro_seconds;
        if (m > e) {
          while (esp_timer_get_time() > e) asm volatile ("nop"); // overflow...
        }
        while (esp_timer_get_time() < e) asm volatile ("nop");
      }
    }

    static void delay(uint32_t milliseconds) {
      if (milliseconds < portTICK_PERIOD_MS) {
        //taskYIELD();
      }
      else {
        vTaskDelay(milliseconds / portTICK_PERIOD_MS);
      }
    }

    static int16_t analog_read(adc1_channel_t channel) {
      adc1_config_width(ADC_WIDTH_BIT_12);
      adc1_config_channel_atten(channel, ADC_ATTEN_MAX);

      return adc1_get_raw(channel);
    }

    static void * ps_malloc(uint32_t size) { 
      void * mem = heap_caps_malloc(size, MALLOC_CAP_SPIRAM); 
      if (mem == nullptr) {
        ESP_LOGE(TAG, "Not enough memory on PSRAM!!! (Asking %u bytes)", size);
      }
      return mem;
    }

    static void show_heaps_info() {
      ESP_LOGD(TAG, "+----- HEAPS DATA -----+");
      ESP_LOGD(TAG, "| Total heap:  %7d |",  heap_caps_get_total_size(MALLOC_CAP_8BIT  ));
      ESP_LOGD(TAG, "| Free heap:   %7d |",    heap_caps_get_free_size(MALLOC_CAP_8BIT  ));
      ESP_LOGD(TAG, "+----------------------+");
    }
};

#endif