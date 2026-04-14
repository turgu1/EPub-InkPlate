#define __WIFI__ 1
#include "controllers/wifi.hpp"

#if EPUB_INKPLATE_BUILD

  #include "freertos/FreeRTOS.h"
  #include "freertos/event_groups.h"
  #include "freertos/task.h"

  #include "esp_event.h"
  #include "esp_system.h"
  #include "esp_wifi.h"

  #include "esp_mac.h"
  #include "lwip/err.h"
  #include "lwip/sys.h"
  #include "mdns.h"
  #include "wifi.hpp"

  static EventGroupHandle_t wifiEventGroup = nullptr;
  static bool wifiFirstStart               = true;

  // ----- wifi_sta_event_handler() -----

  // The event group allows multiple bits for each event, but we
  // only care about two events:
  // - we are connected to the AP with an IP
  // - we failed to connect after the maximum amount of retries

  #define WIFI_CONNECTED_BIT BIT0
  #define WIFI_FAIL_BIT BIT1

  static constexpr int8_t ESP_MAXIMUM_RETRY = 6;
  void wifi_ap_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                             void *event_data) {
    static constexpr char const *TAG = "WiFi AP Event Handler";
    LOG_I("AP Event, Base: %p, Event: %" PRIi32 ".", (void *)event_base, event_id);

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
      wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
      ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
      wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
      ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
  }

  void wifi_sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                              void *event_data) {
    static constexpr char const *TAG = "WiFi STA Event Handler";
    LOG_I("STA Event, Base: %p, Event: %" PRIi32 ".", (void *)event_base, event_id);

    static int retryCount = 0;

    if (event_base == WIFI_EVENT) {
      if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
      } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifiFirstStart) {
          if (retryCount < ESP_MAXIMUM_RETRY) {
            vTaskDelay(pdMS_TO_TICKS(10E3));
            LOG_I("retry to connect to the AP");
            esp_wifi_connect();
            retryCount++;
          } else {
            xEventGroupSetBits(wifiEventGroup, WIFI_FAIL_BIT);
            LOG_I("connect to the AP fail");
          }
        } else {
          LOG_I("Wifi Disconnected.");
          vTaskDelay(pdMS_TO_TICKS(10E3));
          LOG_I("retry to connect to the AP");
          esp_wifi_connect();
        }
      }
    } else if (event_base == IP_EVENT) {
      if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        LOG_I("got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi.setIpAddress(event->ip_info.ip);
        retryCount = 0;
        xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
        wifiFirstStart = false;
      }
    }
  }

  auto WIFI::startSta(void) -> bool {
    if (staRunning) return true;
    if (apRunning) return false;

    bool connected = false;
    wifiFirstStart = true;

    if (wifiEventGroup == nullptr) wifiEventGroup = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_sta_event_handler, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_sta_event_handler, NULL));

    std::string wifiSsid;
    std::string wifiPassword;
    std::string dnsName;

    config.get(Config::Ident::SSID, wifiSsid);
    config.get(Config::Ident::PWD, wifiPassword);
    config.get(Config::Ident::DNS_NAME, dnsName);

    wifi_config_t wifi_config;

    bzero(&wifi_config, sizeof(wifi_config_t));

    wifi_config.sta.bssid_set          = 0;
    wifi_config.sta.pmf_cfg.capable    = true;
    wifi_config.sta.pmf_cfg.required   = false;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    strcpy((char *)wifi_config.sta.ssid, wifiSsid.c_str());
    strcpy((char *)wifi_config.sta.password, wifiPassword.c_str());

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGD(TAG, "startSta() finished.");

    // Waiting until either the connection is established (WIFI_CONNECTED_BIT)
    // or connection failed for the maximum number of re-tries (WIFI_FAIL_BIT).
    // The bits are set by event_handler() (see above)

    EventBits_t bits = xEventGroupWaitBits(wifiEventGroup, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
      LOG_I("connected to ap SSID:%s password:%s", wifiSsid.c_str(), wifiPassword.c_str());
      startMdnsService(dnsName);
      connected = true;
    } else if (bits & WIFI_FAIL_BIT) {
      LOG_E("Failed to connect to SSID:%s, password:%s", wifiSsid.c_str(), wifiPassword.c_str());
    } else {
      LOG_E("UNEXPECTED EVENT");
    }

    if (!connected) {
      ESP_ERROR_CHECK(esp_event_loop_delete_default());
    }

    staRunning = true;
    return connected;
  }

  void WIFI::startMdnsService(const std::string &hostname) {
    // initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
      ESP_LOGE(TAG, "mDNS Init failed: %d\n", err);
      return;
    }

    // set hostname
    if (mdns_hostname_set(hostname.c_str()) != ESP_OK) {
      ESP_LOGE(TAG, "Unable to set mDNS hostname.");
      return;
    }

    // set default instance
    if (mdns_instance_name_set("Inkplate EPub Reader") != ESP_OK) {
      ESP_LOGE(TAG, "Unable to set mDNS instant name.");
      return;
    }

    ESP_LOGD(TAG, "mDNS started.");
    mdnsRunning = true;
  }

  auto WIFI::startAp(void) -> bool {
    if (apRunning) return true;
    if (staRunning) return false;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_ap_event_handler, NULL, NULL));

    std::string apSsid;
    std::string apPassword;
    std::string dnsName;

    uint8_t apSsidRaw[32];
    uint8_t apPasswordRaw[64];

    config.get(Config::Ident::SSID, apSsid);
    config.get(Config::Ident::PWD, apPassword);
    config.get(Config::Ident::DNS_NAME, dnsName);

    wifi_config_t wifi_config;

    // wifi_config.ap.ssid = apSsidRaw;
    // wifi_config.ap.password = apPasswordRaw;
    strncpy((char *)wifi_config.ap.ssid, apSsid.c_str(), 32);
    strncpy((char *)wifi_config.ap.password, apPassword.c_str(), 32);

    wifi_config.ap.ssid_len = apSsid.length();
    wifi_config.ap.channel  = 11;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK, wifi_config.ap.max_connection = 5;
    wifi_config.ap.pmf_cfg.required = false;

    // #ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
    //   wifi_config.ap.authmode = WIFI_AUTH_WPA3_PSK;
    //   wifi_config.ap.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    // #else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
    //   wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    // #endif

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGD(TAG, "startAp() finished. SSID:%s password:%s channel:%d", (char *)apSsidRaw,
             (char *)apPasswordRaw, 11);

    apRunning = true;
    return true;
  }

  void WIFI::stop() {
    vTaskDelay(pdMS_TO_TICKS(500));

    if (mdnsRunning) {
      mdns_free();
    }

    if (staRunning || apRunning) {

      esp_wifi_disconnect();
      esp_wifi_stop();

      // if (staRunning) {
      //   ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT,   ESP_EVENT_ANY_ID,
      //   &wifi_sta_event_handler)); ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT,
      //   WIFI_EVENT_STA_START, &wifi_sta_event_handler));
      // } else {
      //   ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT,   ESP_EVENT_ANY_ID,
      //   &wifi_ap_event_handler));
      // }

      esp_wifi_deinit();
      esp_event_loop_delete_default();

      vEventGroupDelete(wifiEventGroup);
      wifiEventGroup = nullptr;

      staRunning = apRunning = false;
    }
  }

#endif