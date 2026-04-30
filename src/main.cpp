// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __GLOBAL__ 1
#include "global.hpp"

#define TEST_HIMEM 0
#define TEST_DOM 0

#if EPUB_INKPLATE_BUILD
  // InkPlate6 main function and main task

  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"

  #include "alloc.hpp"
  #include "controllers/app_controller.hpp"
  #include "controllers/books_dir_controller.hpp"
  #include "esp.hpp"
  #include "inkplate_platform.hpp"
  #include "models/config.hpp"
  #include "models/epub.hpp"
  #include "models/fonts.hpp"
  #include "models/nvs_mgr.hpp"
  #include "pugixml.hpp"
  #include "screen.hpp"
  #include "viewers/msg_viewer.hpp"

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    #include "controllers/back_lit.hpp"
  #endif

  #include <stdio.h>

  #if TESTING
    #include "gtest/gtest.h"
  #endif

  #if REGRESSION_TEST
    extern void runRegressionLoop();
  #endif

  #include <esp_chip_info.h>
  #include <esp_flash.h>

  static constexpr char const *TAG = "main";

  void mainTask(void *params) {

    // esp_log_level_set("PageLocs", ESP_LOG_DEBUG);

    LOG_D("EPub-Inkplate Startup. (%" PRIi64 ")", esp_timer_get_time());

    bool nvsMgrRes = nvsMgr.setup();

    #if DEBUGGING
      for (int i = 10; i > 0; i--) {
        printf("\r%02d ...", i);
        fflush(stdout);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
      printf("\n");
      fflush(stdout);
    #endif

    #if TESTING
      testing::InitGoogleTest();
      RUN_ALL_TESTS();
      while (1) {
        printf(".");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
      }

    #else
      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
        #define MSG "Press the WakeUp Button to restart."
        #define INT_PIN TouchScreen::INTERRUPT_PIN
        #define LEVEL 0
      #else
        #define MSG "Press a key to restart."
        #if EXTENDED_CASE
          #define INT_PIN PressKeys::INTERRUPT_PIN
        #else
          #define INT_PIN TouchKeys::INTERRUPT_PIN
        #endif
        #define LEVEL 1
      #endif

      bool inkplateErr = !inkplate_platform.setup(true);
      if (inkplateErr) {
        LOG_E("Inkplate Platform Setup Error. Restarting!");
        inkplate_platform.restart();
      }

      bool configErr = !config.read();
      if (configErr) LOG_E("Config Error.");
      #if DATE_TIME_RTC
        else {
          HimemString timeZone;

          config.get(Config::Ident::TIME_ZONE, timeZone);
          setenv("TZ", timeZone.c_str(), 1);
        }
      #endif

      #if DEBUGGING
        config.show();
      #endif

      pugi::set_memory_management_functions(allocate, free);

      // The appFonts only contains the icon and system fonts. Books related fonts are
      // instanciated inside the epub class when a book is open.
      if (appFonts.setup()) {

        Screen::Orientation orientation    = Screen::Orientation::TOP;
        Screen::PixelResolution resolution = Screen::PixelResolution::ONE_BIT;

        config.get(Config::Ident::ORIENTATION, (int8_t *)&orientation);
        config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *)&resolution);

        screen.setup(resolution, orientation);

        eventMgr.setup();
        eventMgr.setOrientation(orientation);

        #if REGRESSION_TEST
          runRegressionLoop();
          while (1) {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
          }
        #endif

        #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
          backLit.setup();
        #endif

        if (!nvsMgrRes) {
          MsgViewer::show(MsgViewer::MsgType::ALERT, false, true, "Hardware Problem!",
                          "Failed to initialise NVS Flash. Entering Deep Sleep. " MSG);

          ESP::delay(500);
          inkplate_platform.deep_sleep(INT_PIN, LEVEL);
        }

        if (configErr) {
          MsgViewer::show(MsgViewer::MsgType::ALERT, false, true, "Configuration Problem!",
                          "Unable to read/save configuration file. Entering Deep Sleep. " MSG);
          ESP::delay(500);
          inkplate_platform.deep_sleep(INT_PIN, LEVEL);
        }

        MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Starting", "One moment please...");

        booksDirController.setup();
        LOG_D("Initialization completed");
        appController.start();
      } else {
        LOG_E("Font loading error.");
        MsgViewer::show(MsgViewer::MsgType::ALERT, false, true, "Font Loading Problem!",
                        "Unable to read required fonts. Entering Deep Sleep. " MSG);
        ESP::delay(500);
        inkplate_platform.deep_sleep(INT_PIN, LEVEL);
      }

      #if DEBUGGING
        while (1) {
          printf("Allo!\n");
          vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
      #endif
    #endif
  }

  #define STACK_SIZE 40000

  extern "C" {

  void app_main(void) {
    // printf("EPub InkPlate Reader Startup\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ", CONFIG_IDF_TARGET, chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    uint32_t flashChipSize;
    esp_flash_get_size(NULL, &flashChipSize);

    printf("%" PRIu32 "MB %s flash\n", flashChipSize / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    heap_caps_print_heap_info(MALLOC_CAP_32BIT | MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM |
                              MALLOC_CAP_INTERNAL);

    UBaseType_t stack_hwm = uxTaskGetStackHighWaterMark(NULL);
    LOG_I("Main task stack high water mark: %u bytes", stack_hwm * sizeof(StackType_t));

    TaskHandle_t xHandle = NULL;

    xTaskCreate(mainTask, "mainTask", STACK_SIZE, (void *)1, configMAX_PRIORITIES - 1, &xHandle);
    configASSERT(xHandle);
  }

  } // extern "C"

#else

  // Linux main function

  #include "controllers/app_controller.hpp"
  #include "controllers/books_dir_controller.hpp"
  #include "helpers/debug_tool.hpp"
  #include "models/config.hpp"
  #include "models/fonts.hpp"
  #include "models/page_locs.hpp"
  #include "screen.hpp"
  #include "viewers/msg_viewer.hpp"

  #if TESTING
    #include "gtest/gtest.h"
  #endif

  static const char *TAG = "Main";

  void exitApp() {
    // appFonts.clearGlyphCaches();
    // appFonts.clear(true);
  }

  auto main(int argc, char **argv) -> int {
    bool configErr = !config.read();
    if (configErr) LOG_E("Config Error.");

    #if DEBUGGING
      config.show();
    #endif

    // DebugTool::printAllClassSizes();

    #if TEST_DOM
      if (!domRunTests()) {
        LOG_E("DOM tests failed.");
      } else {
        LOG_I("Yeah! DOM tests passed!");
      }
    #endif

    if (appFonts.setup()) {

      Screen::Orientation orientation{Screen::Orientation::RIGHT};
      Screen::PixelResolution resolution{Screen::PixelResolution::ONE_BIT};
      config.get(Config::Ident::ORIENTATION, (int8_t *)&orientation);
      config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *)&resolution);
      screen.setup(resolution, orientation);

      eventMgr.setup();
      booksDirController.setup();

      #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    #define MSG "the WakeUp button"
      #else
    #define MSG "a key"
      #endif

      if (configErr) {
        MsgViewer::show(MsgViewer::MsgType::ALERT, false, true, "Configuration Problem!",
                        "Unable to read/save configuration file. Entering Deep Sleep. Press " MSG
                        " to restart.");
        sleep(10);
        exit(0);
      }

      // exit(0)  // Used for some Valgrind tests
      #if TESTING
        testing::InitGoogleTest();
        return RUN_ALL_TESTS();
      #else
        appController.start();
      #endif
    } else {
      MsgViewer::show(MsgViewer::MsgType::ALERT, false, true, "Font Loading Problem!",
                      "Unable to load default fonts.");

      sleep(30);
      return 1;
    }

    return 0;
  }

#endif