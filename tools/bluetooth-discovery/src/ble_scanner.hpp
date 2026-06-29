#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"

// Structure to hold individual device details
struct BleDevice {
  std::string name;
  uint8_t mac[6];
  uint8_t address_type;   // Public, Random, etc.

  std::string get_mac_string() const {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
  }
};

class BleScanner {
  public:
    BleScanner() = default;
    ~BleScanner() = default;

    // Main synchronous scanning execution function
    std::vector<BleDevice> scan(uint32_t duration_ms);

  private:
    // C-compatible static wrapper callback for NimBLE
    static int ble_scan_event_cb(struct ble_gap_event *event, void *arg);

    // Internal instance method to process individual discoveries
    void handle_discovery(struct ble_gap_event *event);

    std::vector<BleDevice> m_discovered_devices;
};
