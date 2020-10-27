#ifndef __ESP_HPP__
#define __ESP_HPP__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define NOP() asm volatile ("nop")

class ESP
{
  public:
    static inline long IRAM_ATTR micros() { return (unsigned long) esp_timer_get_time(); }
    static inline long IRAM_ATTR millis() { return (unsigned long) esp_timer_get_time() / 1000; }

    static void delay_microseconds(uint32_t micro_seconds) {
      uint32_t m = micros();
      if (micro_seconds) {
        uint32_t e = m + micro_seconds;
        if (m > e) {
          while (micros() > e) NOP(); // overflow...
        }
        while (micros() < e) NOP();
      }

    }

    static void delay(uint32_t micro_seconds) {
      vTaskDelay(micro_seconds / portTICK_PERIOD_MS);
    }

    static int16_t analog_read(gpio_num_t port);

    // This needs option CONFIG_SPIRAM_USE in sdconfig to be 
    // set to "Make RAM allocatable using heap_caps_malloc(…, MALLOC_CAP_SPIRAM)" 
    //
    // In sdconfig, the option can be found here:
    //   Component config > ESP32-specific > CONFIG_ESP32_SPIRAM_SUPPORT > SPI RAM config
    //
    static void * ps_malloc(uint32_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
};

#endif