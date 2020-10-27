#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

#include "inkplate6.hpp"

extern "C" {

void app_main() 
{

  while (1) {
    printf("Allo!\n");
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

}