#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

void app_main() 
{

  while (1) {
    printf("Allo!\n");
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}