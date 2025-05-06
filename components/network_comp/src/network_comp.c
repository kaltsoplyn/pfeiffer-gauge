#include "network_comp.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "NetworkComp";
static TimerHandle_t button_timer;
static bool button_pressed = false;



static void button_isr_handler(void *arg) {
    xTimerStart(button_timer, 0);
}

static void button_timer_callback(TimerHandle_t xTimer) {
    button_pressed = true;
    ESP_LOGI(TAG, "Button long press detected - resetting WiFi credentials");
    nvs_erase_wifi_creds();
    esp_restart();
}

// static void wifi_event_handler(void *arg, esp_event_base_t event_base, 
//                              int32_t event_id, void *event_data) {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         ESP_LOGI(TAG, "STA Started");
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         ESP_LOGI(TAG, "STA Disconnected");
//         // Try to reconnect
//         esp_wifi_connect();
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//         ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
//     }
// }

esp_err_t network_comp_init() {
    // Initialize NVS
    esp_err_t ret = nvs_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NVS storage!\n%s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "NVS initialized");
    
    // Initialize WiFi
    ret = wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init WiFi!\n%s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WiFi initialized");
    
    // Check for stored credentials
    char ssid[32] = {0};
    char password[64] = {0};
    
    bool creds_found = nvs_get_wifi_creds(ssid, sizeof(ssid), password, sizeof(password));

    if (creds_found) {
        ESP_LOGI(TAG, "Found stored WiFi credentials, checking availability for SSID: [%s]", ssid);

        // Start SoftAP first. This puts us in APSTA mode and calls esp_wifi_start(),
        // which is necessary for scanning to work.
        ESP_LOGI(TAG, "Temporarily starting SoftAP to enable scanning...");
        esp_err_t softap_ret = wifi_init_softap(); // s_should_connect_on_sta_start will be false
        if (softap_ret == ESP_OK) {
            ESP_LOGI(TAG, "SoftAP started (APSTA mode). Scanning for stored SSID...");
            #define MAX_SCAN_RESULTS 10
            wifi_ap_record_t ap_info[MAX_SCAN_RESULTS];
            uint16_t ap_count = MAX_SCAN_RESULTS;
            bool ssid_found_in_scan = false;

            if (wifi_scan_networks(ap_info, &ap_count) == ESP_OK) {
                for (int i = 0; i < ap_count; i++) {
                    if (strcmp(ssid, (char *)ap_info[i].ssid) == 0) {
                        ssid_found_in_scan = true;
                        ESP_LOGI(TAG, "Stored SSID [%s] found in scan results.", ssid);
                        break;
                    }
                }
            } else {
                ESP_LOGW(TAG, "Scan failed. Will attempt connection to stored SSID [%s] anyway.", ssid);
                ssid_found_in_scan = true; // Fallback: try to connect if scan fails
            }

            if (ssid_found_in_scan) {
                ESP_LOGI(TAG, "Attempting to connect to stored SSID: [%s]", ssid);
                // wifi_connect_sta will set s_should_connect_on_sta_start = true
                // and then set mode to WIFI_MODE_STA, effectively stopping the AP part.
                esp_err_t sta_connect_ret = wifi_connect_sta(ssid, password);
                if (sta_connect_ret != ESP_OK) {
                    ESP_LOGW(TAG, "Connection to stored SSID [%s] failed (%s). Reverting to AP mode.", ssid, esp_err_to_name(sta_connect_ret));
                    // Explicitly re-initialize SoftAP to ensure it's active
                    ret = wifi_init_softap(); // This will set s_should_connect_on_sta_start = false
                } else {
                    // Connection successful
                    ret = ESP_OK;
                }
                // If ret is ESP_OK, STA connection is initiated.
            } else {
                ESP_LOGW(TAG, "Stored SSID [%s] not found in scan. Device remains in AP mode.", ssid);
                // AP mode is already active from the successful wifi_init_softap() call.
                // 'ret' should reflect the success of wifi_init_softap() in this path.
                ret = softap_ret; // Which is ESP_OK
            }
        } else { // wifi_init_softap() failed
            ESP_LOGE(TAG, "Failed to start SoftAP for scanning. Error: %s. Cannot proceed with STA attempt.", esp_err_to_name(softap_ret));
            // If wifi_init_softap fails here, we can't scan.
            // 'ret' should carry this failure.
            ret = softap_ret;
        }
    } else {
        ESP_LOGI(TAG, "No stored WiFi credentials, starting config AP");
        ret = wifi_init_softap(); // s_should_connect_on_sta_start will be false
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start SoftAP in no-creds mode: %s", esp_err_to_name(ret));
            // Error will be returned by the function
        }
    }

    // Only start web server if previous Wi-Fi setup steps didn't report a critical failure
    // or if we intend to start it anyway (e.g., for AP mode).
    // The current logic implies web server should start regardless.
    esp_err_t web_server_ret = start_web_server();
    if (web_server_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server!\n%s", esp_err_to_name(web_server_ret));
        return web_server_ret; // Prioritize web server start error if Wi-Fi was OK
    }
    
    // Initialize button for reset
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init GPIO!\n%s", esp_err_to_name(ret));
        return ret;
    }
    
    button_timer = xTimerCreate("button_timer", pdMS_TO_TICKS(CONFIG_BUTTON_PRESS_MS), pdFALSE, NULL, button_timer_callback);
    if (button_timer == NULL) ret = ESP_FAIL;

    ret = ret == ESP_OK ? gpio_install_isr_service(0) : ret;
    ret = ret == ESP_OK ? gpio_isr_handler_add(CONFIG_BUTTON_GPIO, button_isr_handler, NULL) : ret;

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init GPIO!\n%s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}