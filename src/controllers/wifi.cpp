#define __WIFI__ 1
#include "controllers/wifi.hpp"

#if EPUB_INKPLATE_BUILD

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi.hpp"
#include "mdns.h"
#include "esp_mac.h"


static EventGroupHandle_t wifi_event_group = nullptr;
static bool               wifi_first_start =    true;

// ----- wifi_sta_event_handler() -----

// The event group allows multiple bits for each event, but we 
// only care about two events:
// - we are connected to the AP with an IP
// - we failed to connect after the maximum amount of retries

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static constexpr int8_t ESP_MAXIMUM_RETRY = 6;
void 
wifi_ap_event_handler(void             * arg, 
                      esp_event_base_t   event_base,
                      int32_t            event_id, 
                      void             * event_data)
{
  static constexpr char const * TAG = "WiFi AP Event Hanfler";
  LOG_I("AP Event, Base: %p, Event: %" PRIi32 ".", (void *) event_base, event_id);

  if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
    ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
              MAC2STR(event->mac), event->aid);
  } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
    ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
              MAC2STR(event->mac), event->aid);
  }
}

void 
wifi_sta_event_handler(void             * arg, 
                       esp_event_base_t   event_base,
                       int32_t            event_id, 
                       void             * event_data)
{
  static constexpr char const * TAG = "WiFi STA Event Hanfler";
  LOG_I("STA Event, Base: %p, Event: %" PRIi32 ".", (void *) event_base, event_id);

  static int s_retry_num = 0;

  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();
    } 
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      if (wifi_first_start) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
          vTaskDelay(pdMS_TO_TICKS(10E3));
          LOG_I("retry to connect to the AP");
          esp_wifi_connect();
          s_retry_num++;
        } 
        else {
          xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
          LOG_I("connect to the AP fail");
        }
      }
      else {
        LOG_I("Wifi Disconnected.");
        vTaskDelay(pdMS_TO_TICKS(10E3));
        LOG_I("retry to connect to the AP");
        esp_wifi_connect();
      }
    } 
  }
  else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t * event = (ip_event_got_ip_t*) event_data;
      LOG_I("got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      wifi.set_ip_address(event->ip_info.ip);
      s_retry_num = 0;
      xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
      wifi_first_start = false;
    }
  }
}

bool 
WIFI::start_sta(void)
{
  if (sta_running) return true;
  if (ap_running) return false;

  bool connected = false;
  wifi_first_start = true;

  if (wifi_event_group == nullptr) wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(
    WIFI_EVENT, 
    ESP_EVENT_ANY_ID,    
    &wifi_sta_event_handler, 
    NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(
    IP_EVENT,   
    IP_EVENT_STA_GOT_IP, 
    &wifi_sta_event_handler, 
    NULL));

  std::string wifi_ssid;
  std::string wifi_pwd;
  std::string dns_name;

  config.get(Config::Ident::SSID,     wifi_ssid);
  config.get(Config::Ident::PWD,      wifi_pwd );
  config.get(Config::Ident::DNS_NAME, dns_name );

  wifi_config_t wifi_config;

  bzero(&wifi_config, sizeof(wifi_config_t));

  wifi_config.sta.bssid_set          = 0;
  wifi_config.sta.pmf_cfg.capable    = true;
  wifi_config.sta.pmf_cfg.required   = false;
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  strcpy((char *) wifi_config.sta.ssid,     wifi_ssid.c_str());
  strcpy((char *) wifi_config.sta.password, wifi_pwd.c_str());

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGD(TAG, "start_sta() finished.");

  // Waiting until either the connection is established (WIFI_CONNECTED_BIT) 
  // or connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). 
  // The bits are set by event_handler() (see above)

  EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
    pdFALSE,
    pdFALSE,
    portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT) {
    LOG_I("connected to ap SSID:%s password:%s",
            wifi_ssid.c_str(), wifi_pwd.c_str());
    start_mdns_service(dns_name);
    connected = true;
  } 
  else if (bits & WIFI_FAIL_BIT) {
    LOG_E("Failed to connect to SSID:%s, password:%s",
            wifi_ssid.c_str(), wifi_pwd.c_str());
  }
  else {
    LOG_E("UNEXPECTED EVENT");
  }

  if (!connected) {
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
  }

  sta_running = true;
  return connected;
}

void WIFI::start_mdns_service(const std::string &dns_name)
{
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "mDNS Init failed: %d\n", err);
        return;
    }

    //set hostname
    if (mdns_hostname_set(dns_name.c_str()) != ESP_OK) {
      ESP_LOGE(TAG, "Unable to set mDNS hostname.");
      return;
    }

    //set default instance
    if (mdns_instance_name_set("Inkplate EPub Reader") != ESP_OK) {
      ESP_LOGE(TAG, "Unable to set mDNS instant name.");
      return;
    }

    ESP_LOGD(TAG, "mDNS started.");
    mdns_running = true;
}

bool 
WIFI::start_ap(void)
{
  if (ap_running) return true;
  if (sta_running) return false;

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_ap_event_handler,
                                                      NULL,
                                                      NULL));

  std::string ap_ssid;
  std::string ap_pwd;
  std::string dns_name;

  uint8_t my_ap_ssid[32];
  uint8_t my_ap_pwd[64];

  config.get(Config::Ident::SSID,     ap_ssid);
  config.get(Config::Ident::PWD,      ap_pwd );
  config.get(Config::Ident::DNS_NAME, dns_name );

  wifi_config_t wifi_config;

  // wifi_config.ap.ssid = my_ap_ssid;
  // wifi_config.ap.password = my_ap_pwd;
  strncpy((char *)wifi_config.ap.ssid, ap_ssid.c_str(), 32);
  strncpy((char *)wifi_config.ap.password, ap_pwd.c_str(), 32);

  wifi_config.ap.ssid_len = ap_ssid.length();
  wifi_config.ap.channel = 11;
  wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK,
  wifi_config.ap.max_connection = 5;
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

  ESP_LOGD(TAG, "start_ap() finished. SSID:%s password:%s channel:%d",
            (char *)my_ap_ssid, (char *)my_ap_pwd, 11);

  ap_running = true;
  return true;
}

void
WIFI::stop()
{
  vTaskDelay(pdMS_TO_TICKS(500));

  if (mdns_running) {
    mdns_free();
  }

  if (sta_running || ap_running) {

    esp_wifi_disconnect();
    esp_wifi_stop();

    // if (sta_running) {
    //   ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT,   ESP_EVENT_ANY_ID,     &wifi_sta_event_handler));
    //   ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_sta_event_handler));
    // } else {
    //   ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT,   ESP_EVENT_ANY_ID,     &wifi_ap_event_handler));
    // }

    esp_wifi_deinit();
    esp_event_loop_delete_default();

    vEventGroupDelete(wifi_event_group);
    wifi_event_group = nullptr;

    sta_running = ap_running = false;
  }
}

#endif