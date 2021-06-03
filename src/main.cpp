// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __GLOBAL__ 1
#include "global.hpp"

#if EPUB_INKPLATE_BUILD

  // InkPlate6 main function and main task
  
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "logging.hpp"

  #include "controllers/books_dir_controller.hpp"
  #include "controllers/app_controller.hpp"
  #include "models/fonts.hpp"
  #include "models/epub.hpp"
  #include "models/config.hpp"
  #include "screen.hpp"
  #include "inkplate_platform.hpp"
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
    LOG_I("EPub-Inkplate Startup.");
    
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err != ESP_OK) {
      if ((nvs_err == ESP_ERR_NVS_NO_FREE_PAGES) || (nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        LOG_D("Erasing NVS Partition... (Because of %s)", esp_err_to_name(nvs_err));
        if ((nvs_err = nvs_flash_erase()) == ESP_OK) {
          nvs_err = nvs_flash_init();
        }
      }
    } 
    if (nvs_err != ESP_OK) LOG_E("NVS Error: %s", esp_err_to_name(nvs_err));

    #if DEBUGGING
      for (int i = 10; i > 0; i--) {
        printf("\r%02d ...", i);
        fflush(stdout);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
      printf("\n"); fflush(stdout);
    #endif

    bool inkplate_err = !inkplate_platform.setup(true);
    if (inkplate_err) LOG_E("InkPlate6Ctrl Error.");

    bool config_err = !config.read();
    if (config_err) LOG_E("Config Error.");

    #if DEBUGGING
      config.show();
    #endif

    pugi::set_memory_management_functions(allocate, free);

    page_locs.setup();

    if (fonts.setup()) {
      
      Screen::Orientation    orientation;
      Screen::PixelResolution resolution;
      config.get(Config::Ident::ORIENTATION,      (int8_t *) &orientation);
      config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *) &resolution);
      screen.setup(resolution, orientation);

      event_mgr.setup();
      event_mgr.set_orientation(orientation);

      if (nvs_err != ESP_OK) {
        msg_viewer.show(MsgViewer::ALERT, false, true, "Hardware Problem!",
          "Failed to initialise NVS Flash (%s). Entering Deep Sleep. Press a key to restart.",
           esp_err_to_name(nvs_err)
        );

        ESP::delay(500);
        inkplate_platform.deep_sleep();
      }
  
      if (inkplate_err) {
        msg_viewer.show(MsgViewer::ALERT, false, true, "Hardware Problem!",
          "Unable to initialize the InkPlate drivers. Entering Deep Sleep. Press a key to restart."
        );
        ESP::delay(500);
        inkplate_platform.deep_sleep();
      }

      if (config_err) {
        msg_viewer.show(MsgViewer::ALERT, false, true, "Configuration Problem!",
          "Unable to read/save configuration file. Entering Deep Sleep. Press a key to restart."
        );
        ESP::delay(500);
        inkplate_platform.deep_sleep();
      }

      books_dir_controller.setup();
      LOG_D("Initialization completed");
      app_controller.start();
    }
    else {
      LOG_E("Font loading error.");
      msg_viewer.show(MsgViewer::ALERT, false, true, "Font Loading Problem!",
        "Unable to read required fonts. Entering Deep Sleep. Press a key to restart."
      );
      ESP::delay(500);
      inkplate_platform.deep_sleep();
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

      xTaskCreate(mainTask, "mainTask", STACK_SIZE, (void *) 1, configMAX_PRIORITIES - 1, &xHandle);
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
  #include "models/page_locs.hpp"
  #include "screen.hpp"

  static const char * TAG = "Main";

  int 
  main(int argc, char **argv) 
  {
    bool config_err = !config.read();
    if (config_err) LOG_E("Config Error.");

    #if DEBUGGING
      config.show();
    #endif

    page_locs.setup();
    
    if (fonts.setup()) {

      Screen::Orientation    orientation;
      Screen::PixelResolution resolution;
      config.get(Config::Ident::ORIENTATION,     (int8_t *) &orientation);
      config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *) &resolution);
      screen.setup(resolution, orientation);

      event_mgr.setup();
      books_dir_controller.setup();

      if (config_err) {
        msg_viewer.show(MsgViewer::ALERT, false, true, "Configuration Problem!",
          "Unable to read/save configuration file. Entering Deep Sleep. Press a key to restart."
        );
        sleep(10);
        exit(0);
      }

      // exit(0)  // Used for some Valgrind tests
      app_controller.start();
    }
    else {
      msg_viewer.show(MsgViewer::ALERT, false, true, "Font Loading Problem!",
        "Unable to load default fonts."
      );

      sleep(30);
      return 1;

    }
    
    return 0;
  }

#endif