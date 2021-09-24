// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __GLOBAL__ 1
#include "global.hpp"

#if EPUB_INKPLATE_BUILD
  // InkPlate6 main function and main task
  
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"

  #include "controllers/books_dir_controller.hpp"
  #include "controllers/app_controller.hpp"
  #include "models/fonts.hpp"
  #include "models/epub.hpp"
  #include "models/config.hpp"
  #include "models/nvs_mgr.hpp"
  #include "screen.hpp"
  #include "inkplate_platform.hpp"
  #include "helpers/unzip.hpp"
  #include "viewers/msg_viewer.hpp"
  #include "pugixml.hpp"
  #include "alloc.hpp"
  #include "esp.hpp"

  #include <stdio.h>

  #if TESTING
    #include "gtest/gtest.h"
  #endif

  static constexpr char const * TAG = "main";

  void 
  mainTask(void * params) 
  {
    LOG_I("EPub-Inkplate Startup.");
    
    bool nvs_mgr_res = nvs_mgr.setup();

    #if DEBUGGING
      for (int i = 10; i > 0; i--) {
        printf("\r%02d ...", i);
        fflush(stdout);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
      printf("\n"); fflush(stdout);
    #endif

    #if TESTING

      testing::InitGoogleTest();
      RUN_ALL_TESTS();
      while (1) {
        printf(".");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
      }
    
    #else
      bool inkplate_err = !inkplate_platform.setup(true);
      if (inkplate_err) LOG_E("InkPlate6Ctrl Error.");

      bool config_err = !config.read();
      if (config_err) LOG_E("Config Error.");

      #if DEBUGGING
        config.show();
      #endif

      pugi::set_memory_management_functions(allocate, free);

      page_locs.setup();

      #if EXTENDED_CASE
        #define INT_PIN PressKeys::INTERRUPT_PIN
      #else
        #define INT_PIN TouchKeys::INTERRUPT_PIN
      #endif

      if (fonts.setup()) {
        
        Screen::Orientation    orientation;
        Screen::PixelResolution resolution;
        config.get(Config::Ident::ORIENTATION,      (int8_t *) &orientation);
        config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *) &resolution);
        screen.setup(resolution, orientation);

        event_mgr.setup();
        event_mgr.set_orientation(orientation);

        if (!nvs_mgr_res) {
          msg_viewer.show(MsgViewer::ALERT, false, true, "Hardware Problem!",
            "Failed to initialise NVS Flash. Entering Deep Sleep. Press a key to restart."
          );

          ESP::delay(500);
          inkplate_platform.deep_sleep(INT_PIN, 1);
        }
    
        if (inkplate_err) {
          msg_viewer.show(MsgViewer::ALERT, false, true, "Hardware Problem!",
            "Unable to initialize the InkPlate drivers. Entering Deep Sleep. Press a key to restart."
          );
          ESP::delay(500);
          inkplate_platform.deep_sleep(INT_PIN, 1);
        }

        if (config_err) {
          msg_viewer.show(MsgViewer::ALERT, false, true, "Configuration Problem!",
            "Unable to read/save configuration file. Entering Deep Sleep. Press a key to restart."
          );
          ESP::delay(500);
          inkplate_platform.deep_sleep(INT_PIN, 1);
        }

        msg_viewer.show(MsgViewer::INFO, false, true, "Starting", "One moment please...");

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
        inkplate_platform.deep_sleep(INT_PIN, 1);
      }

      #if DEBUGGING
        while (1) {
          printf("Allo!\n");
          vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
      #endif
    #endif
  }

  #define STACK_SIZE 60000

  extern "C" {
    
    void 
    app_main(void)
    {
      //printf("EPub InkPlate Reader Startup\n");

      /* Print chip information */
      esp_chip_info_t chip_info;
      esp_chip_info(&chip_info);
      printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
              CONFIG_IDF_TARGET,
              chip_info.cores,
              (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
              (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

      printf("silicon revision %d, ", chip_info.revision);

      printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
              (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

      printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

      heap_caps_print_heap_info(MALLOC_CAP_32BIT|MALLOC_CAP_8BIT|MALLOC_CAP_SPIRAM|MALLOC_CAP_INTERNAL);

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

  #if TESTING
    #include "gtest/gtest.h"
  #endif

  static const char * TAG = "Main";

  void exit_app()
  {
    fonts.clear_glyph_caches();
    fonts.clear(true);
    epub.close_file();
    DOM::delete_pool();
  }

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

      Screen::Orientation     orientation;
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
      #if TESTING
        testing::InitGoogleTest();
        return RUN_ALL_TESTS();
      #else
        app_controller.start();
      #endif
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