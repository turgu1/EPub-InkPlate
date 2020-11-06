#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.hpp"

#include <stdio.h>

#include "inkplate6_ctrl.hpp"
#include "page.hpp"
#include "eink.hpp"

static const char *TAG = "main";

void 
mainTask(void * params) 
{
  for (int i = 10; i > 0; i--) {
    printf("\r%02d ...", i);
    fflush(stdout);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  printf("\n"); fflush(stdout);

#if 1
  if (!inkplate6_ctrl.setup() || 
      !fonts.setup()) {
    LOG_E(TAG, "Unable to setup the hardware environment!");
  }

  LOG_D(TAG, "Initialization completed");

  Page::Format fmt = {
    .line_height_factor = 0.9,
    .font_index         = 0,
    .font_size          = CSS::font_normal_size,
    .indent             = 0,
    .margin_left        = 0,
    .margin_right       = 0,
    .margin_top         = 0,
    .margin_bottom      = 0,
    .screen_left        = 10,
    .screen_right       = 10,
    .screen_top         = 10,
    .screen_bottom      = 10,
    .width              = 0,
    .height             = 0,
    .trim               = true,
    .font_style         = Fonts::NORMAL,
    .align              = CSS::LEFT_ALIGN,
    .text_transform     = CSS::NO_TRANSFORM
  };

  page.set_compute_mode(Page::DISPLAY);

  page.start(fmt);
  page.new_paragraph(fmt);
  page.add_word("Allo!!", fmt);
  page.end_paragraph(fmt);
  page.paint();
#endif

  // e_ink.setup();
  // e_ink.clean();

  while (1) {
    printf("Allo!\n");
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

#define STACK_SIZE 20000

extern "C" {

void 
app_main(void)
{
  TaskHandle_t xHandle = NULL;

  xTaskCreate(mainTask, "mainTask", STACK_SIZE, (void *) 1, tskIDLE_PRIORITY, &xHandle);
  configASSERT(xHandle);

  //esp_task_wdt_add(xHandle);
}

} // extern "C"