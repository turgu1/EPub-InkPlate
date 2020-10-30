#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.hpp"

#include <stdio.h>

#include "inkplate6_ctrl.hpp"
#include "Graphics.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

extern "C" {

static const char * TAG = "Main";

void app_main() 
{
  // esp_log_level_set("*", ESP_LOG_DEBUG);

  for (int i = 10; i > 0; i--) {
    printf("\r%02d ...", i);
    fflush(stdout);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  printf("\n"); fflush(stdout);

  ESP_LOGD(TAG, "Initializations...");

  if (e_ink.initialize()) {
    ESP_LOGI(TAG, "EInk initialized!");
  }

  Graphics graphics;

  graphics.setDisplayMode(3);
  graphics.writeLine(10, 10, 200, 200, BLACK);
  graphics.show();
  
  while (1) {
    printf("Allo!\n");
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

}