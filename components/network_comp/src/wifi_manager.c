#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include <string.h>
#include "esp_mac.h"
#include "freertos/event_groups.h" // For potential future use with connection status bits

static const char *TAG = "WiFiManager";
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;
static bool wifi_initialized = false;
static bool s_should_connect_on_sta_start = false; // Flag to control auto-connection

// Event group to signal Wi-Fi connection events
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT      = BIT1; // Could be from disconnect or timeout

// Forward declaration for the event handler
void wifi_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data);

esp_err_t wifi_init() {
    if (wifi_initialized) {
        ESP_LOGI(TAG, "WiFi already initialized.");
        return ESP_OK;
    }

    esp_err_t ret = esp_netif_init();
    ret = ret == ESP_OK ? esp_event_loop_create_default() : ret;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init event loop: %s", esp_err_to_name(ret));
        // No return here, as other parts of wifi_init might still be useful
        // or we might want to proceed to AP mode.
        // The caller of wifi_init should check its return.
    }

    // Create default STA and AP interfaces here, they can be configured later
    sta_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();
    if (!sta_netif || !ap_netif) {
        ESP_LOGE(TAG, "Failed to create default interfaces");
        return ESP_FAIL;
    }

    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create wifi_event_group");
        return ESP_FAIL; // Critical failure
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);

    // Register event handlers
    ret = ret == ESP_OK ? esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL) 
                                                        : ret;
    ret = ret == ESP_OK ? esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL)
                                                        : ret;

    // Note: You might need to unregister these if you implement deinitialization
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handlers: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized");

    return ESP_OK;
}

esp_err_t wifi_init_softap(void) {
    // When starting SoftAP, we generally don't want the STA part to auto-connect
    // unless explicitly told to by a subsequent wifi_connect_sta call.
    s_should_connect_on_sta_start = false;
    if (!wifi_initialized) wifi_init();

    // First create the AP interface
    // ap_netif should already be created in wifi_init()

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_AP_SSID,
            .ssid_len = strlen(CONFIG_AP_SSID),
            .channel = CONFIG_AP_CHANNEL,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    // Set mode to APSTA to allow scanning and potential STA connection later
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    ret = ret == ESP_OK ? esp_wifi_set_config(WIFI_IF_AP, &wifi_config) : ret;

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi AP config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return ret; // Don't proceed if start fails
    }
    
    ESP_LOGI(TAG, "SoftAP initialized. SSID: %s at %s:%d", CONFIG_AP_SSID, CONFIG_AP_IP_ADDR, CONFIG_WEB_PORT);

    return ESP_OK;
}

esp_err_t wifi_scan_networks(wifi_ap_record_t *ap_info, uint16_t *ap_count) {
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    // Ensure WiFi is started and in a mode that allows scanning (APSTA or STA)
    wifi_mode_t current_mode;
    if (esp_wifi_get_mode(&current_mode) != ESP_OK || (current_mode != WIFI_MODE_STA && current_mode != WIFI_MODE_APSTA)) {
         ESP_LOGE(TAG, "WiFi is not in STA or APSTA mode (current mode: %d), cannot scan.", current_mode);
         return ESP_ERR_WIFI_MODE; // Return a more specific error
    }

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(err));
        return err; // Return the error code
    }
    err = esp_wifi_scan_get_ap_records(ap_count, ap_info);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Get AP records failed: %s", esp_err_to_name(err)); // Log error name
    return err;
}

esp_err_t wifi_connect_sta(const char *ssid, const char *password) {
    // Signal that when STA_START event occurs, we should indeed try to connect.
    s_should_connect_on_sta_start = true;

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    if (sta_netif == NULL) {
        sta_netif = esp_netif_create_default_wifi_sta();
        if (sta_netif == NULL) {
            ESP_LOGE(TAG, "Failed to create default STA interface");
            return ESP_FAIL;
        }
    }
    
    // Stop Wi-Fi first to ensure a clean start for STA mode
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_stop() failed before STA mode set: %s. Attempting to continue.", esp_err_to_name(ret));
        // Continue, as setting mode and starting might still work or recover.
    }
    
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    ret = ret == ESP_OK ? esp_wifi_set_config(WIFI_IF_STA, &wifi_config) : ret;
    ret = ret == ESP_OK ? esp_wifi_start() : ret;

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi setting mode STA failed: %s", esp_err_to_name(ret));
        return ret; // Return the error code
    }

    ESP_LOGI(TAG, "Setting STA config: SSID=[%s], Password=[%s]", ssid, strlen(password) > 0 ? "****" : "!EMPTY!");

    ESP_LOGI(TAG, "Attempting to connect to SSID: %s", ssid);
    // esp_wifi_connect() will be called automatically by the event handler
    // upon WIFI_EVENT_STA_START, so commenting out the call below

    // Wait for connection result using event group
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdTRUE, // Clear bits on exit
            pdFALSE, // Wait for EITHER bit (any)
            pdMS_TO_TICKS(15000)); // 15-second timeout for connection

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "Connection attempt failed (disconnected or other failure event).");
        return ESP_FAIL; // Or a more specific error like ESP_ERR_WIFI_CONN
    } else {
        ESP_LOGW(TAG, "Connection attempt timed out for SSID: %s", ssid);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;

}

bool wifi_is_connected(void) {
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

bool wifi_get_ip_address(char *ip_addr) {
    esp_netif_ip_info_t ip_info;

    // First, try to get the STA IP address if the interface exists and has an IP
    if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        // Check if the IP is valid (not 0.0.0.0)
        if (ip_info.ip.addr != 0) {
            snprintf(ip_addr, 16, IPSTR, IP2STR(&ip_info.ip));
            //ESP_LOGI(TAG, "STA IP address resolved: %s", ip_addr);
            return true;
        }
    }
    // If STA is not connected or has no IP, try to get the AP IP address
    if (ap_netif && esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
        snprintf(ip_addr, 16, IPSTR, IP2STR(&ip_info.ip));
        //ESP_LOGI(TAG, "AP IP address resolved: %s", ip_addr);
        return true;
    }

    ESP_LOGW(TAG, "IP address could not be retrieved for STA or AP.");
    return false;
}

// Event handler for WiFi and IP events
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START: Station started.");
        if (s_should_connect_on_sta_start) {
            ESP_LOGI(TAG, "Initiating connection attempt...");
            esp_wifi_connect(); // Initiate connection when STA interface starts AND flag is set
        } else {
            ESP_LOGI(TAG, "STA started, but connection not initiated by flag.");
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        // ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED: Disconnected from AP SSID: %s, reason: %d",
        //          event->ssid, event->reason);
        const char* reason_str;
        switch (event->reason) {
            case WIFI_REASON_AUTH_EXPIRE:
                reason_str = "Auth Expired";
                break;
            case WIFI_REASON_AUTH_FAIL:
                reason_str = "Auth Failed";
                break;
            case WIFI_REASON_NO_AP_FOUND:
                reason_str = "No AP Found";
                break;
            case WIFI_REASON_ASSOC_FAIL:
                reason_str = "Association Failed";
                break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT:
                reason_str = "Handshake Timeout";
                break;
            default:
                reason_str = "Unknown";
        }
        ESP_LOGW(TAG, "Disconnected from AP. SSID: %.*s, Reason: %s (%d)", 
                event->ssid_len, event->ssid, reason_str, event->reason);
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        // Optional: Implement retry logic here if desired
        // ESP_LOGI(TAG, "Attempting to reconnect due to disconnection...");
        // esp_wifi_connect(); // Temporarily comment out immediate retry to see original error
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP: Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        // Optional: Set a flag or signal that connection is successful
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED: Station " MACSTR " joined, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED: Station " MACSTR " left, AID=%d", MAC2STR(event->mac), event->aid);
    }
}