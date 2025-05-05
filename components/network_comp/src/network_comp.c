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
    
    // Initialize WiFi
    ret = wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init WiFi!\n%s", esp_err_to_name(ret));
        return ret;
    }
    
    // Check for stored credentials
    char ssid[32] = {0};
    char password[64] = {0};
    
    if (nvs_get_wifi_creds(ssid, sizeof(ssid), password, sizeof(password))) {
        ESP_LOGI(TAG, "Found stored WiFi credentials, connecting...");
        ESP_LOGI(TAG, "  -> SSID: [%s], Password: [%s]", ssid, sizeof(password) > 0 ? "****" : "! ZERO LENGTH PASSWORD !"); // Log credentials being used
        wifi_connect_sta(ssid, password);
    } else {
        ESP_LOGI(TAG, "No stored WiFi credentials, starting config AP");
        wifi_init_softap();
    }

    ret = start_web_server();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server!\n%s", esp_err_to_name(ret));
        return ret;
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