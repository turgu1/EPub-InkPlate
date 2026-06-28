#if BLE_KEYPAD

  #pragma once

  #include "global.hpp"

  #include "esp_bt.h"
  #include "esp_bt_main.h"
  #include "esp_gap_ble_api.h"
  #include "esp_gatt_common_api.h"
  #include "esp_gattc_api.h"

  #define TRACING_BLE_KEYPAD 0

  class BLEKeypad {
    private:
      static constexpr const char *TAG = "BLEKeypad";

      static BLEKeypad *instance;

      static constexpr uint8_t TARGET_MAC_ADDRESS[] = { 0x2a, 0x07, 0x98, 0x74, 0x8a, 0x1b };

      uint8_t macAddress[6];
      uint8_t keysMapping[6][5]{}; // 6 buttons, each with 5 bytes of data
      uint16_t debouncingTimeMs{ 500 }; // 500 ms debounce time

      esp_bd_addr_t gl_remote_bda{ 0 };
      esp_gatt_if_t gl_gattc_if{ 0 };

      bool is_connecting{ false };
      bool is_if_registered{ false };

      uint16_t gl_conn_id{ 0 };

      // Time tracking variable for the 500 ms debounce lock
      int64_t last_button_press_time{ 0 };

      // Standard SIG BLE UUID Definitions for HID Devices
      static constexpr uint16_t HID_REPORT_CHAR_UUID = 0x2A4D;
      static constexpr uint16_t BLE_CCCD_UUID        = 0x2902;

      esp_ble_scan_params_t scan_params = { .scan_type          = BLE_SCAN_TYPE_ACTIVE,
                                            .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
                                            .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
                                            .scan_interval      = 0x50,
                                            .scan_window        = 0x30,
                                            .scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE };

      // TikTok Remote Control

      // Sticky routing latch to separate Up from Down before the identical mouse packet arrives
      enum class VerticalIntent { INTENT_NONE, INTENT_UP, INTENT_DOWN };
      VerticalIntent pendingVerticalIntent{ VerticalIntent::INTENT_NONE };

      // Latch mechanisms to filter multiple stream copies within a single physical burst
      bool isVerticalPressed{ false };
      // bool isCenterPressed{false};

      auto hexToInt(const char c) const -> uint8_t;
      auto parseHexStringToBytes(const char *hexStr, uint8_t *byteArray, size_t byteArraySize,
                                 char separator) const -> void;

      auto handleIncomingPacket(uint8_t *data, size_t length) -> void;

      auto gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) -> void;

      auto gattcEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                             esp_ble_gattc_cb_param_t *param) -> void;

      static void gapEventHandlerStub(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
        if (instance) {
          instance->gapEventHandler(event, param);
        }
      }

      static void gattcEventHandlerStub(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
        if (instance) {
          instance->gattcEventHandler(event, gattc_if, param);
        }
      }

    public:
      BLEKeypad()  = default;
      ~BLEKeypad() = default;

      auto setup() -> bool;
  };

  #if __BLE_KEYPAD__
    BLEKeypad bleKeypad;
  #else
    extern BLEKeypad bleKeypad;
  #endif

#endif // BLE_KEYPAD
