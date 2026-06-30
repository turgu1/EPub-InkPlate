#if BLE_KEYPAD

  #define __BLE_KEYPAD__ 1
  #include "ble_keypad.hpp"

  #include "esp_timer.h"
//   #include "nvs_flash.h"
  #include "esp_log.h"
  #include "nimble/nimble_port.h"
  #include "nimble/nimble_port_freertos.h"
  #include "host/ble_uuid.h"

  BLEKeypad *BLEKeypad::instance = nullptr;

  auto BLEKeypad::hexToInt(const char c) const -> uint8_t {
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    if (c >= '0' && c <= '9') { return c - '0'; }
    return 0;
  }

  auto BLEKeypad::parseHexStringToBytes(const char *hexStr, uint8_t *byteArray, size_t byteArraySize,
                                        char separator) const -> void {
    for (size_t i = 0; i < byteArraySize; i++) {
      byteArray[i] = 0;
      while (*hexStr && *hexStr != separator) {
        byteArray[i] = (byteArray[i] << 4) | hexToInt(*hexStr);
        hexStr++;
      }
      if (*hexStr == separator) {
        hexStr++;
      }
    }
  }

  // The handleIncomingPacket is taylored to the specific packet structures emitted by the TikTok Remote
  // Control, which is the primary target device for this BLE keypad integration. It decodes the raw
  // HID report data into actionable events while managing internal state to handle the unique burst
  // patterns of the device's input stream.
  //
  // The remote control can be purchased through AliExpress:
  //
  // https://www.aliexpress.com/item/1005007944515439.html
  //

  #define DO while (1)
  #define END_DO break

  auto BLEKeypad::handleIncomingPacket(uint8_t *data, size_t length) -> void {
    if (length == 0 || data == nullptr) { return; }

    auto reset_state = [&]() {
                         pendingVerticalIntent = VerticalIntent::INTENT_NONE;
                         isVerticalPressed     = false;
                       };

    Event event = { EventKind::NONE };

    #if TRACING_BLE_KEYPAD
      LOG_I("---- [handleIncomingPacket] ----");
      LOG_I("Received BLE Packet. Length: {}", (int)length);
      for (size_t i = 0; i < length; i++) {
        LOG_I("  {}: 0x{:02x} ", (int)i, data[i]);
      }
    #endif

    DO {
      if (length == 2) {
        uint8_t reportId   = data[0];
        uint8_t clickValue = data[1];

        if ((reportId == 1 || reportId == 2) && clickValue == 0) {
          event.kind = EventKind::DBL_SELECT;
          reset_state();
          break;
        }
        break;
      }

      if (length < 4) { break; }
      uint8_t reportId = data[0];

      if (reportId == 7) {
        uint8_t marker = data[3];

        if (marker == 0xF0) {
          if (pendingVerticalIntent == VerticalIntent::INTENT_NONE) {
            pendingVerticalIntent = VerticalIntent::INTENT_UP;
          }
          break;
        }
        if (marker == 0xF8) {
          if (pendingVerticalIntent == VerticalIntent::INTENT_NONE) {
            pendingVerticalIntent = VerticalIntent::INTENT_DOWN;
          }
          break;
        }
        break;
      }

      if (reportId == 0) {
        int8_t deltaX = (int8_t)(data[1]);
        int8_t deltaY = (int8_t)(data[2]);

        if (deltaX == 0 && deltaY == 0) {
          break;
        }

        if (deltaX == (int8_t)0xE0 && deltaY == 6) {
          event.kind = EventKind::PREV;
          reset_state();
          break;
        }

        if (deltaX == 0x78 && deltaY == 0) {
          event.kind = EventKind::NEXT;
          reset_state();
          break;
        }

        if (deltaX == (int8_t)0xD0 && deltaY == (int8_t)0xF9) {
          if (!isVerticalPressed) {
            isVerticalPressed = true;
            if (pendingVerticalIntent == VerticalIntent::INTENT_UP) {
              event.kind = EventKind::DBL_PREV;
            } else if (pendingVerticalIntent == VerticalIntent::INTENT_DOWN) {
              event.kind = EventKind::DBL_NEXT;
            }
            reset_state();
          }
          pendingVerticalIntent = VerticalIntent::INTENT_NONE;
          isVerticalPressed = false;
          break;
        }
        break;
      }

      if (reportId == 4) {
        uint8_t targetByte = data[1];
        if (targetByte == 0xF4) {
          event.kind = EventKind::SELECT;
        }
      }

      END_DO;
    }

    #if TRACING_BLE_KEYPAD
      LOG_I("Vertical Pressed State: {}", isVerticalPressed ? "Yes" : "No");
      LOG_I("-------------------------------");
    #endif

    if (event.kind != EventKind::NONE) {
      if (bleEventQueue) {
        xQueueSend(bleEventQueue, &event, 0);
      } else {
        LOG_E("Event bleEventQueue not initialized. Unable to send event.");
      }
    }
  }

  // --- NIMBLE SCAN ENGINE MANAGER ---
  auto BLEKeypad::startScanning() -> void {
    if (is_connecting) { return; }

    struct ble_gap_disc_params disc_params;
    memset(&disc_params, 0, sizeof(disc_params));

    disc_params.filter_policy     = BLE_HCI_SCAN_FILT_NO_WL;
    disc_params.passive           = 0; // Active scanning captures scan responses
    disc_params.itvl              = 0x0050;
    disc_params.window            = 0x0030;

    // FIX 1: Turn on duplicate filtering to stop catch every packet burst
    disc_params.filter_duplicates = 1;

    // FIX 2: Change duration argument from 0 to BLE_HS_FOREVER for continuous scanning
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params,
                          BLEKeypad::gapEventStub, nullptr);
    if (rc != 0) {
      LOG_E("Error initiating NimBLE discovery scan; rc={}", rc);
    } else {
      LOG_W("=== BLE SCANNER RUNNING ===");
    }
  }

  // --- NIMBLE CENTRAL GAP EVENT LOOP ---
  auto BLEKeypad::handleGapEvent(struct ble_gap_event *event) -> int {
    int rc;

    switch (event->type) {
    // Replaces legacy Bluedroid: ESP_GAP_BLE_SCAN_RESULT_EVT
    case BLE_GAP_EVENT_DISC: {
      // Safe, unrolled manual byte matching evaluating NimBLE LSB order
      bool match = true;
      for (int i = 0; i < 6; i++) {
        // Compare forward target elements against backward NimBLE values
        if (event->disc.addr.val[5 - i] != TARGET_MAC_ADDRESS[i]) {
          match = false;
          break;
        }
      }

      if (match) {
        if (!is_connecting) {
          is_connecting = true;
          LOG_W("TARGET MAC MATCHED! Terminating scan and establishing connection...");

          ble_gap_disc_cancel();

          rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr, 30000,
                               nullptr, BLEKeypad::gapEventStub, nullptr);
          if (rc != 0) {
            LOG_E("Failed to initiate device pairing connection; rc={}", rc);
            is_connecting = false;
            startScanning();
          }
        }
      }
      return 0;
    }

    // Replaces legacy Bluedroid: ESP_GATTC_CONNECT_EVT & ESP_GATTC_OPEN_EVT
    case BLE_GAP_EVENT_CONNECT: {
      if (event->connect.status == 0) {
        LOG_I("GATT Channel Active! Pulling internal service maps...");
        gl_conn_id = event->connect.conn_handle;
        is_connecting = false;

        ble_uuid16_t hid_uuid = {
          .u = { .type = BLE_UUID_TYPE_16 },
          .value = HID_REPORT_CHAR_UUID
        };

        // Ground-level discovery pass passing discoveryStub to register the endpoints
        rc = ble_gattc_disc_chrs_by_uuid(gl_conn_id, 1, 0xffff, &hid_uuid.u,
                                         BLEKeypad::discoveryStub, nullptr);
        if (rc != 0) {
          LOG_E("GATT query initialization failure; rc={}", rc);
        }
      } else {
        LOG_E("Failed to map GATT channel, error status: {}", event->connect.status);
        is_connecting = false;
        startScanning();
      }
      return 0;
    }


    // Replaces legacy Bluedroid: ESP_GATTC_DISCONNECT_EVT
    case BLE_GAP_EVENT_DISCONNECT: {
      LOG_W("BLE Device disconnected. Re-opening scanning state machine...");
      gl_conn_id = BLE_HS_CONN_HANDLE_NONE;
      is_connecting = false;
      startScanning();
      return 0;
    }

    case BLE_GAP_EVENT_NOTIFY_RX: {
      // Only process notifications coming from our connected remote device
      if (event->notify_rx.conn_handle == gl_conn_id) {

        #if TRACING_BLE_KEYPAD
          LOG_I("Notification received on attribute handle {}", event->notify_rx.attr_handle);
        #endif

        // Forward the raw memory payload straight into your static C++ bridging stub
        BLEKeypad::onNotifyStub(event->notify_rx.conn_handle,
                                event->notify_rx.attr_handle,
                                event->notify_rx.om,
                                nullptr);
      }
      return 0;
    }

    default:
      break;
    }
    return 0;
  }

  // --- GATT CHARACTERISTIC PARSER LOOP ---
  auto BLEKeypad::handleDiscovery(const struct ble_gatt_chr *chr) -> void {
    if (chr->properties & BLE_GATT_CHR_PROP_NOTIFY) {
      LOG_W("Found valid HID Notification Handle at: {}. Activating stream...", chr->val_handle);

      uint16_t cccd_handle = chr->val_handle + 1;
      uint8_t  value[] = { 0x01, 0x00 };

      LOG_I("Writing CCCD descriptor to subscribe to notifications...");

      // FIX: Use subscriptionStub here to handle the write confirmation event safely
      int rc = ble_gattc_write_flat(gl_conn_id, cccd_handle, value, sizeof(value),
                                    BLEKeypad::subscriptionStub, nullptr);
      if (rc != 0) {
        LOG_E("Error trying to update peripheral notification permissions; rc={}", rc);
      }
    }
  }

  // --- NOTIFICATION REGISTRATION TRACKER ---
  auto BLEKeypad::handleSubscription(int status, uint16_t attr_handle) -> void {
    if (status == 0) {
      LOG_I("CCCD descriptor written successfully. Handle {} subscribed!", attr_handle);

      // REMOVED: The recursive ble_gattc_disc_chrs_by_uuid call.
      // The stream is already live. Doing nothing here breaks the infinite query loop!
    } else {
      LOG_E("Descriptor subscription mapping error returned; status={}", status);
    }
  }

  // auto BLEKeypad::handleSubscription(int status, uint16_t attr_handle) -> void {
  //   if (status == 0) {
  //     LOG_I("CCCD descriptor written successfully. Device subscribed!");

  //     // C++ COMPATIBLE RESOLUTION: Explicit stack object allocation
  //     ble_uuid16_t hid_uuid = {
  //       .u = { .type = BLE_UUID_TYPE_16 },
  //       .value = HID_REPORT_CHAR_UUID
  //     };

  //     // Provide the clean type-casted address pointer natively
  //     ble_gattc_disc_chrs_by_uuid(gl_conn_id, 1, 0xffff, &hid_uuid.u,
  //                                 nullptr, nullptr);
  //   } else {
  //     LOG_E("Descriptor subscription mapping error returned; status={}", status);
  //   }
  // }

  // --- HARDWARE STACK INITIALIZER ---
  auto BLEKeypad::setup(QueueHandle_t eventQueue) -> bool {
    instance      = this;
    bleEventQueue = eventQueue;

    // 1. Initialize NVS (NimBLE requires NVS for store/bonding data tracking structures)
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //   if (esp_nvs_flash_erase() != ESP_OK) { return false; }
    //   ret = nvs_flash_init();
    // }
    // if (ret != ESP_OK) { return false; }

    // 2. Initialize NimBLE platform controller driver configurations
    if (nimble_port_init() != ESP_OK) {
      return false;
    }

    // 3. Security parameters context mapping (Aligns with original pairing parameters)
    ble_hs_cfg.reset_cb   = blecent_on_reset;
    ble_hs_cfg.sync_cb    = blecent_on_sync;
    ble_hs_cfg.sm_io_cap  = BLE_SM_IO_CAP_NO_IO; // Replaces legacy ESP_IO_CAP_NONE
    ble_hs_cfg.sm_bonding = 1;                   // Replaces legacy ESP_LE_AUTH_BOND

    // 4. Create the background tracking thread daemon task execution context
    xTaskCreate(blecent_host_task, "blecent_host_task", 4096, nullptr, 5, nullptr);

    return true;
  }

  // --- GLOBAL SYSTEM CONTEXT LINKAGE WRAPPERS ---
  void BLEKeypad::blecent_on_reset(int reason) {
    ESP_LOGE("BLEKeypad", "Resetting NimBLE stack host context; reason={}", reason);
  }

  void BLEKeypad::blecent_on_sync(void) {
    ESP_LOGI("BLEKeypad", "GATT Client registered. NimBLE host and controller sync achieved.");
    if (instance) {
      instance->startScanning(); // Perfectly accessible now!
    }
  }

  void BLEKeypad::blecent_host_task(void *param) {
    ESP_LOGI("BLEKeypad", "NimBLE core thread tracking initiated.");
    nimble_port_run();
    nimble_port_freertos_deinit();
  }

#endif
