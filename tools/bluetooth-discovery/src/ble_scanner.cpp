#include "ble_scanner.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

std::vector<BleDevice> BleScanner::scan(uint32_t duration_ms) {
  m_discovered_devices.clear();

  struct ble_gap_disc_params disc_params;
  std::memset(&disc_params, 0, sizeof(disc_params));

  disc_params.filter_duplicates = 1;
  disc_params.passive = 0;   // Use active scanning to query device names via scan responses
  disc_params.itvl = BLE_GAP_SCAN_ITVL_MS(100);
  disc_params.window = BLE_GAP_SCAN_WIN_MS(50);

  // Pass 'this' pointer as the last argument so the callback can access this instance
  int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, duration_ms, &disc_params,
                        BleScanner::ble_scan_event_cb, this);
  if (rc != 0) {
    return m_discovered_devices;
  }

  // Block thread synchronously while NimBLE gathers results via internal events
  vTaskDelay(pdMS_TO_TICKS(duration_ms + 200));

  return m_discovered_devices;
}

// Static function maps incoming data back into our C++ instance
int BleScanner::ble_scan_event_cb(struct ble_gap_event *event, void *arg) {
  BleScanner *scannerInstance = static_cast<BleScanner *>(arg);

  if (event->type == BLE_GAP_EVENT_DISC && scannerInstance != nullptr) {
    scannerInstance->handle_discovery(event);
  }
  return 0;
}

void BleScanner::handle_discovery(struct ble_gap_event *event) {
  // Deduplicate: check if this MAC address is already in our vector array
  for (const auto &dev : m_discovered_devices) {
    if (std::memcmp(dev.mac, event->disc.addr.val, 6) == 0) {
      return;
    }
  }

  struct ble_hs_adv_fields fields;
  std::string              device_name = "Unknown BLE Device";

  if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) == 0) {
    if (fields.name != nullptr) {
      device_name.assign(reinterpret_cast<const char *>(fields.name), fields.name_len);
    }
  }

  // Construct and push the new object directly onto the vector stack
  BleDevice device;
  device.name = device_name;
  device.address_type = event->disc.addr.type;
  std::memcpy(device.mac, event->disc.addr.val, 6);

  m_discovered_devices.push_back(device);
}
