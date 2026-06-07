#if BLE_KEYPAD

  #define __BLE_KEYPAD__ 1
  #include "ble_keypad.hpp"

  #include "esp_timer.h" // Required for microsecond uptime tracking
  #include "freertos/FreeRTOS.h"
  #include "freertos/queue.h"
  #include "freertos/task.h"

  #include "controllers/event_mgr.hpp"

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

    // State management reset callback
    auto reset_state = [&]() {
                         pendingVerticalIntent = VerticalIntent::INTENT_NONE;
                         isVerticalPressed     = false;
                       };

    EventMgr::Event event = { EventMgr::EventKind::NONE };

    #if TRACING_BLE_KEYPAD
      LOG_I("---- [handleIncomingPacket] ----");
      LOG_I("Received BLE Packet: ID={}, Data=", data[0]);
      for (size_t i = 0; i < length; i++) {
        LOG_I("  {}: {:#04x} ", i, data[i]);
      }
    #endif

    DO {
      // --- CATEGORY 1: SHORT DOUBLE-TAP MODIFIER (Length 2) ---
      if (length == 2) {
        uint8_t reportId   = data[0];
        uint8_t clickValue = data[1];

        if ((reportId == 1 || reportId == 2) && clickValue == 0) {
          event.kind = EventMgr::EventKind::DBL_SELECT;
          // action     = "Double Click";
          reset_state();
          break;
        }
        break;
      }

      if (length < 4) { break; }
      uint8_t reportId = data[0];

      // --- CATEGORY 2: CONSUMER CONTROL (VERTICAL INTENT LOGGING) ---
      if (reportId == 7) {
        uint8_t marker = data[3];

        // Up key burst always initiates with an 0xF0 at byte 3
        if (marker == 0xF0) {
          if (pendingVerticalIntent == VerticalIntent::INTENT_NONE) {
            pendingVerticalIntent = VerticalIntent::INTENT_UP;
          }
          break;
        }
        // Down key burst always initiates with an 0xF8 at byte 3
        if (marker == 0xF8) {
          if (pendingVerticalIntent == VerticalIntent::INTENT_NONE) {
            pendingVerticalIntent = VerticalIntent::INTENT_DOWN;
          }
          break;
        }
        break;
      }

      // --- CATEGORY 3: MOUSE DATA PROCESSING (THE ACTION LAYER) ---
      if (reportId == 0) {
        int8_t deltaX = (int8_t)(data[1]);
        int8_t deltaY = (int8_t)(data[2]);

        // Standard mouse rest packet (00 00) handles horizontal clear logic
        if (deltaX == 0 && deltaY == 0) {
          break;
        }

        // A. Left Arrow: Decodes immediately on 00 E0 06 50 06
        if (deltaX == (int8_t)0xE0 && deltaY == 6) {
          event.kind = EventMgr::EventKind::PREV;
          // action     = "Left Click";
          reset_state();
          break;
        }

        // B. Right Arrow: Decodes immediately on 00 78 00 50 06
        if (deltaX == 0x78 && deltaY == 0) {
          event.kind = EventMgr::EventKind::NEXT;
          // action     = "Right Click";
          reset_state();
          break;
        }

        // C. Vertical Execution: Decodes immediately on 00 D0 F9 24 0B
        if (deltaX == (int8_t)0xD0 && deltaY == (int8_t)0xF9) {
          if (!isVerticalPressed) {
            isVerticalPressed = true; // Lock the execution frame state against duplicate burst frames
            if (pendingVerticalIntent == VerticalIntent::INTENT_UP) {
              event.kind = EventMgr::EventKind::DBL_PREV;
              // action     = "Up Click";
            } else if (pendingVerticalIntent == VerticalIntent::INTENT_DOWN) {
              event.kind = EventMgr::EventKind::DBL_NEXT;
              // action     = "Down Click";
            }
            reset_state();
          }

          // Instantly clean up the vertical pipeline states.
          pendingVerticalIntent = VerticalIntent::INTENT_NONE;
          isVerticalPressed = false;
          break;
        }
        break;
      }

      // --- CATEGORY 4: CENTER COMPONENT ---
      if (reportId == 4) {
        uint8_t targetByte = data[1];
        if (targetByte == 0xF4) {
          // action     = "Center Click";
          event.kind = EventMgr::EventKind::SELECT;
        }
      }

      END_DO;
    }

    #if TRACING_BLE_KEYPAD
      LOG_I("Vertical Pressed State: {}", isVerticalPressed ? "Yes" : "No");
      LOG_I("-------------------------------");
    #endif

    if (event.kind != EventMgr::EventKind::NONE) {
      auto queue = eventMgr.getBLEKeypadEventQueue();
      if (queue) {
        // Non-blocking queue dispatch safely integrated into the FreeRTOS engine
        xQueueSend(queue, &event, 0);
      } else {
        LOG_E("Event queue not initialized. Unable to send event.");
      }
    }
  }

  auto BLEKeypad::gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) -> void {

    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      LOG_I("GAP parameters configured. Starting Active BLE Scan...");
      esp_ble_gap_start_scanning(0);
      break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
        LOG_W("=== BLE SCANNER RUNNING ===");
      } else {
        LOG_E("Scan start failed: {}", param->scan_start_cmpl.status);
      }
      break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
        if (memcmp(param->scan_rst.bda, TARGET_MAC_ADDRESS, 6) == 0) {
          if (!is_connecting && is_if_registered) {
            is_connecting = true;
            LOG_W("TARGET MAC MATCHED! Starting connection...");
            esp_ble_gap_stop_scanning();
            esp_ble_gattc_open(gl_gattc_if, param->scan_rst.bda, param->scan_rst.ble_addr_type, true);
          }
        }
      }
      break;
    }
    default:
      break;
    }
  }

  auto BLEKeypad::gattcEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) -> void {
    switch (event) {
    case ESP_GATTC_REG_EVT:
      gl_gattc_if      = gattc_if;
      is_if_registered = true;
      LOG_I("GATT Client registered. ID: {}", gattc_if);
      break;

    case ESP_GATTC_CONNECT_EVT:
      LOG_I("GATT Link established. Initializing pairing security layers...");
      memcpy(gl_remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
      esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_NO_MITM);
      break;

    case ESP_GATTC_OPEN_EVT:
      if (param->open.status != ESP_GATT_OK) {
        LOG_E("Failed to map GATT channel, error status: {}", param->open.status);
        is_connecting = false;
      } else {
        gl_conn_id = param->open.conn_id;
        LOG_I("GATT Channel Active! Pulling internal service maps...");
        esp_ble_gattc_search_service(gattc_if, param->open.conn_id, NULL);
      }
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      LOG_I("Service search complete. Locating HID Input Report to subscribe...");

      uint16_t          count = 0;
      esp_gatt_status_t status =
        esp_ble_gattc_get_attr_count(gattc_if, param->search_cmpl.conn_id,
                                     ESP_GATT_DB_CHARACTERISTIC, 0x0001, 0xFFFF, 0, &count);
      if (status != ESP_GATT_OK || count == 0) {
        LOG_E("No characteristics found on the device database.");
        break;
      }

      esp_gattc_char_elem_t *char_elem_result =
        (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
      if (!char_elem_result) {
        LOG_E("Out of memory allocating characteristic buffer.");
        break;
      }

      esp_bt_uuid_t target_char_uuid = { .len  = ESP_UUID_LEN_16,
                                         .uuid = { .uuid16 = HID_REPORT_CHAR_UUID } };

      status = esp_ble_gattc_get_char_by_uuid(gattc_if, param->search_cmpl.conn_id, 0x0001, 0xFFFF,
                                              target_char_uuid, char_elem_result, &count);

      if (status == ESP_GATT_OK) {
        for (int i = 0; i < count; i++) {
          if (char_elem_result[i].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
            LOG_W("Found valid HID Notification Handle at: {}. Activating stream...",
                  char_elem_result[i].char_handle);

            esp_ble_gattc_register_for_notify(gattc_if, gl_remote_bda,
                                              char_elem_result[i].char_handle);

            uint16_t notify_en   = 0x0001;
            uint16_t descr_count = 0;
            esp_ble_gattc_get_attr_count(gattc_if, param->search_cmpl.conn_id, ESP_GATT_DB_DESCRIPTOR,
                                         char_elem_result[i].char_handle, 0xFFFF, 0, &descr_count);

            if (descr_count > 0) {
              esp_gattc_descr_elem_t *descr_elem =
                (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * descr_count);
              if (descr_elem) {
                esp_bt_uuid_t descr_uuid = { .len  = ESP_UUID_LEN_16,
                                             .uuid = { .uuid16 = BLE_CCCD_UUID } };

                status = esp_ble_gattc_get_descr_by_uuid(gattc_if, param->search_cmpl.conn_id, 0x0001,
                                                         0xFFFF, target_char_uuid, descr_uuid,
                                                         descr_elem, &descr_count);

                if (status == ESP_GATT_OK && descr_count > 0) {
                  esp_ble_gattc_write_char_descr(
                    gattc_if, param->search_cmpl.conn_id, descr_elem->handle, sizeof(notify_en),
                    (uint8_t *)&notify_en, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                  LOG_I("CCCD descriptor written successfully. Device subscribed!");
                }
                free(descr_elem);
              }
            }
          }
        }
      }
      free(char_elem_result);
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      LOG_I("Local subscription pipeline tracking initialized successfully.");
      break;

    case ESP_GATTC_NOTIFY_EVT: {
      uint8_t *data = param->notify.value;
      size_t   len    = param->notify.value_len;

      if (len == 0 || data == NULL) { break; }

      handleIncomingPacket(data, len);

      break;
    }

    case ESP_GATTC_DISCONNECT_EVT:
      LOG_W("Beauty-R1 disconnected. Re-opening scanning state machine...");
      is_connecting = false;
      esp_ble_gap_start_scanning(0);
      break;

    default:
      break;
    }
  }

  #define CHECK(expr)                                                                                \
          if (ESP_OK != ESP_ERROR_CHECK_WITHOUT_ABORT(expr)) {                                             \
            return false;                                                                                  \
          }

  auto BLEKeypad::setup() -> bool {

    instance = this;

    CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    CHECK(esp_bt_controller_init(&bt_cfg));
    CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    CHECK(esp_bluedroid_init());
    CHECK(esp_bluedroid_enable());

    CHECK(esp_ble_gap_register_callback(gapEventHandlerStub));
    CHECK(esp_ble_gattc_register_callback(gattcEventHandlerStub));
    CHECK(esp_ble_gattc_app_register(0));

    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
    esp_ble_io_cap_t   iocap      = ESP_IO_CAP_NONE;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,      &iocap,    sizeof(uint8_t));

    CHECK(esp_ble_gap_set_scan_params(&scan_params));

    return true;
  }

#endif
