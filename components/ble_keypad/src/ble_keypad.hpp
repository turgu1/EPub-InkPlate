#if BLE_KEYPAD

  #pragma once

  #include "global.hpp"

  #include "freertos/FreeRTOS.h"
  #include "freertos/queue.h"
  #include "freertos/task.h"

// Apache NimBLE Host and Port APIs for ESP-IDF v5.5.x
  #include "nimble/ble.h"
  #include "modlog/modlog.h"
  #include "host/ble_hs.h"
  #include "host/ble_gap.h"
  #include "services/gap/ble_svc_gap.h"

  #define TRACING_BLE_KEYPAD 0

  class BLEKeypad {

    private:
      static constexpr const char *TAG = "BLEKeypad";

      // Singleton pointer used by static C callbacks to route back into C++ object context
      static BLEKeypad *instance;

      enum class EventKind { NONE, NEXT, PREV, DBL_NEXT, DBL_PREV, SELECT, DBL_SELECT };
      struct Event {
        EventKind kind;
      };

      QueueHandle_t bleEventQueue{};

      static constexpr uint8_t TARGET_MAC_ADDRESS[] = { 0x2a, 0x07, 0x98, 0x74, 0x8a, 0x1b };

      uint8_t macAddress[6];
      uint8_t keysMapping[6][5]{};
      uint16_t debouncingTimeMs{ 500 };

      // NimBLE tracks connection sessions using connection handles instead of interface objects
      uint16_t gl_conn_id{ BLE_HS_CONN_HANDLE_NONE };
      bool is_connecting{ false };

      // Time tracking variable for the 500 ms debounce lock
      int64_t last_button_press_time{ 0 };

      // Standard SIG BLE UUID Definitions for HID Devices
      static constexpr uint16_t HID_REPORT_CHAR_UUID = 0x2A4D;
      static constexpr uint16_t BLE_CCCD_UUID        = 0x2902;

      // TikTok Remote Control context tracking
      enum class VerticalIntent { INTENT_NONE, INTENT_UP, INTENT_DOWN };
      VerticalIntent pendingVerticalIntent{ VerticalIntent::INTENT_NONE };
      bool isVerticalPressed{ false };

      auto hexToInt(const char c) const -> uint8_t;
      auto parseHexStringToBytes(const char *hexStr, uint8_t *byteArray, size_t byteArraySize,
                                 char separator) const -> void;

      auto handleIncomingPacket(uint8_t *data, size_t length) -> void;

      // --- C++ NimBLE Class Instance Handlers ---
      auto handleGapEvent(struct ble_gap_event *event) -> int;
      auto handleDiscovery(const struct ble_gatt_chr *chr) -> void;
      auto handleSubscription(int status, uint16_t attr_handle) -> void;

      // --- Static Bridging Stubs (Links NimBLE C function pointers to C++ instance variables) ---
      static int gapEventStub(struct ble_gap_event *event, void *arg) {
        if (instance) {
          return instance->handleGapEvent(event);
        }
        return 0;
      }

      static int discoveryStub(uint16_t conn_handle, const struct ble_gatt_error *error,
                               const struct ble_gatt_chr *chr, void *arg) {

        // 1. Check if the discovery procedure completed or encountered an error
        if (error->status != 0) {
          if (error->status == BLE_HS_EDONE) {
            ESP_LOGI("BLEKeypad", "Characteristic discovery process completed successfully.");
          } else {
            ESP_LOGE("BLEKeypad", "Error during characteristic discovery; status=%d", error->status);
          }
          return 0; // Return 0 to tell NimBLE the callback executed fine
        }

        // 2. If a valid characteristic was discovered, pass it to your class handler
        if (chr != nullptr && instance != nullptr) {
          instance->handleDiscovery(chr);
        }

        return 0;
      }

      static int subscriptionStub(uint16_t conn_handle, const struct ble_gatt_error *error,
                                  struct ble_gatt_attr *attr, void *arg) { // <-- Changed to ble_gatt_attr
        if (instance) {
          instance->handleSubscription(error->status, attr->handle);
        }
        return 0;
      }


      static int onNotifyStub(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om, void *arg) {
        if (instance && om) {
          // Safely flatten the NimBLE segmented mbuf memory chain down into a contiguous buffer
          uint16_t len = OS_MBUF_PKTLEN(om);
          if (len > 0) {
            uint8_t *data = (uint8_t *)malloc(len);
            if (data) {
              os_mbuf_copydata(om, 0, len, data);
              instance->handleIncomingPacket(data, len);
              free(data);
            }
          }
        }
        return 0;
      }

    public:
      BLEKeypad()  = default;
      ~BLEKeypad() = default;

      auto setup(QueueHandle_t eventQueue) -> bool;
      auto startScanning() -> void;

      static void blecent_on_reset(int reason);
      static void blecent_on_sync(void);
      static void blecent_host_task(void *param);
  };

  #if __BLE_KEYPAD__
    BLEKeypad bleKeypad;
  #else
    extern BLEKeypad bleKeypad;
  #endif

#endif // BLE_KEYPAD
