// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __GLOBAL__ 1
#include "global.hpp"

#if EPUB_INKPLATE6_BUILD

  // InkPlate6 main function and main task
  
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "logging.hpp"

  #include "controllers/books_dir_controller.hpp"
  #include "controllers/app_controller.hpp"
  #include "models/fonts.hpp"
  #include "screen.hpp"
  #include "inkplate6_ctrl.hpp"
  #include "models/epub.hpp"
  #include "models/config.hpp"
  #include "helpers/unzip.hpp"
  #include "viewers/msg_viewer.hpp"
  #include "pugixml.hpp"
  #include "nvs_flash.h"
  #include "alloc.hpp"
  #include "esp.hpp"

  #include <stdio.h>

  static constexpr char const * TAG = "main";

  void 
  mainTask(void * params) 
  {
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
      if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        LOG_D("Erasing NVS Partition... (Because of %s)", esp_err_to_name(err));
        if ((err = nvs_flash_erase()) == ESP_OK) {
          err = nvs_flash_init();
        }
      }
      if (err != ESP_OK) {
        MsgViewer::show(MsgViewer::ALERT, false, true, "Hardware Problem!",
          "Failed to initialise NVS Flash (%s). Entering Deep Sleep. Press a key to restart.",
           esp_err_to_name(err)
        );
        ESP::delay(500);
        inkplate6_ctrl.deep_sleep();
      }
    } 

    #if DEBUGGING
      for (int i = 10; i > 0; i--) {
        printf("\r%02d ...", i);
        fflush(stdout);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
      printf("\n"); fflush(stdout);
    #endif

    if (!inkplate6_ctrl.setup()) {
      MsgViewer::show(MsgViewer::ALERT, false, true, "Hardware Problem!",
        "Unable to initialize InkPlate-6 drivers. Entering Deep Sleep. Press a key to restart."
      );
      ESP::delay(500);
      inkplate6_ctrl.deep_sleep();
    }

    if (!config.read()) {
      MsgViewer::show(MsgViewer::ALERT, false, true, "Configuration Problem!",
        "Unable to read/save configuration file. Entering Deep Sleep. Press a key to restart."
      );
      ESP::delay(500);
      inkplate6_ctrl.deep_sleep();
    }

    #if DEBUGGING
      config.show();
    #endif

    pugi::set_memory_management_functions(allocate, free);

    if (fonts.setup()) {
      screen.setup();
      event_mgr.setup();
      books_dir_controller.setup();
      LOG_D("Initialization completed");
      app_controller.start();
    }
    else {
      MsgViewer::show(MsgViewer::ALERT, false, true, "Font Loading Problem!",
        "Unable to read required fonts. Entering Deep Sleep. Press a key to restart."
      );
      ESP::delay(500);
      inkplate6_ctrl.deep_sleep();
    }

    #if DEBUGGING
      while (1) {
        printf("Allo!\n");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
      }
    #endif
  }

  #define STACK_SIZE 40000

  extern "C" {

    void 
    app_main(void)
    {
      TaskHandle_t xHandle = NULL;

      xTaskCreate(mainTask, "mainTask", STACK_SIZE, (void *) 1, tskIDLE_PRIORITY, &xHandle);
      configASSERT(xHandle);
    }

  } // extern "C"

#else

  // Linux main function

  #include "controllers/books_dir_controller.hpp"
  #include "controllers/app_controller.hpp"
  #include "viewers/msg_viewer.hpp"
  #include "models/fonts.hpp"
  #include "models/config.hpp"
  #include "screen.hpp"

  static const char * TAG = "Main";

  int 
  main(int argc, char **argv) 
  {
    if (!config.read()) {
      MsgViewer::show(MsgViewer::ALERT, false, true, "Configuration Problem!",
        "Unable to read/save configuration file."
      );

      sleep(30);
      return 1;
    }

    #if DEBUGGING
      config.show();
    #endif

    if (fonts.setup()) {
      screen.setup();
      event_mgr.setup();
      books_dir_controller.setup();
      // exit(0)  // Used for some Valgrind tests
      app_controller.start();
    }
    else {
      MsgViewer::show(MsgViewer::ALERT, false, true, "Font Loading Problem!",
        "Unable to load default fonts."
      );

      sleep(30);
      return 1;

    }
    
    return 0;
  }

#endif