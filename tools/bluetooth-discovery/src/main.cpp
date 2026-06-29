// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __GLOBAL__ 1
#include "global.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "freertos/semphr.h"

#include "alloc.hpp"
#include "esp.hpp"
#include "inkplate_platform.hpp"
#include "screen.hpp"

#include "ble_scanner.hpp"

#include <stdio.h>

#include <esp_chip_info.h>

static constexpr char const *TAG = "main";

SemaphoreHandle_t            ble_sync_semaphore{ nullptr };

void nimble_host_task(void *param) {
  printf("NimBLE Host Task Started\n");

  // This blocks the task thread and handles all background packet traffic
  nimble_port_run();

  nimble_port_freertos_deinit();
}

void mainTask(void *params) {

  // esp_log_level_set("PageLocs", ESP_LOG_DEBUG);

  LOG_I("BLE Scanner Startup. ({})", esp_timer_get_time());

  // 1. Initialize NVS Flash (Still mandatory for storing pairing data keys)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Create the binary semaphore BEFORE starting the stack
  ble_sync_semaphore = xSemaphoreCreateBinary();

  // 2. Initialize the NimBLE Host, Controller, and HCI layers combined
  // In ESP-IDF 5.x, this single call handles what used to take 2-3 functions.
  int rc = nimble_port_init();
  if (rc != 0) {
    printf("Failed to initialize nimble port, error code: %d\n", rc);
    return;
  }

  // 3. Configure synchronization status callback handlers
  ble_hs_cfg.sync_cb = []() {
                         printf("NimBLE Host successfully synchronized with the Controller!\n");
                         xSemaphoreGive(ble_sync_semaphore); // Unblocks app_main
                       };

  // 4. Create the FreeRTOS host thread execution loop
  nimble_port_freertos_init(nimble_host_task);

  // Explicitly wait here forever until the semaphore is given
  printf("Waiting for NimBLE stack to synchronize...\n");
  xSemaphoreTake(ble_sync_semaphore, portMAX_DELAY);

  #define MSG "Press the WakeUp Button to restart."
  #define INT_PIN EInk::WAKE_UP_PIN
  #define LEVEL 0


  // Initialize the platform first, to be able to use its delay and
  // deep sleep functions in case of critical errors.
  bool inkplateErr = !inkplate_platform.setup(true);
  if (inkplateErr) {
    LOG_E("Inkplate Platform Setup Error. Restarting!");
    inkplate_platform.restart();
  }

  Screen::Orientation     orientation    = Screen::Orientation::TOP;
  Screen::PixelResolution resolution = Screen::PixelResolution::ONE_BIT;

  screen.setup(resolution, orientation);


  // if (!bleKeypad.setup()) {
  //   MsgViewer::show(
  //     MsgViewer::MsgType::ALERT, false, true, "Bluetooth Problem!",
  //     "Failed to initialize Bluetooth. Bluetooth features will be unavailable. " MSG);
  // }

  BleScanner scanner;

  // Execute a standard 5-second discovery window loop
  std::vector<BleDevice> devices = scanner.scan(5000);

  printf("\n\n==== BLE Devices ===========================\n\nFound %u unique BLE devices:\n", devices.size());
  for (const auto &device : devices) {
    printf("Device: %s | MAC: %s\n",
           device.name.c_str(),
           device.get_mac_string().c_str());
  }

  printf("\n==== End ===================================\n\n");

  while (1) {
    printf("Allo!\n");
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
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

    printf("silicon revision %d, ",                       chip_info.revision);

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    heap_caps_print_heap_info(MALLOC_CAP_32BIT | MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM |
                              MALLOC_CAP_INTERNAL);

    UBaseType_t stack_hwm = uxTaskGetStackHighWaterMark(NULL);
    LOG_I("Main task stack high water mark: {} bytes", stack_hwm * sizeof(StackType_t));

    TaskHandle_t xHandle = NULL;

    xTaskCreate(mainTask, "mainTask", STACK_SIZE, (void *)1, configMAX_PRIORITIES - 1, &xHandle);
    configASSERT(xHandle);
  }

}   // extern "C"