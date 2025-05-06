#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_AP_SSID         "ESP32-C6-Config"
#define CONFIG_AP_CHANNEL      6
#define CONFIG_AP_IP_ADDR      "192.168.4.1"
#define CONFIG_WEB_PORT        80
#define CONFIG_DATA_PORT       6666

/**
 * @brief Initialize WiFi basic checks
 */
esp_err_t wifi_init(void);

/**
 * @brief Initialize WiFi in Access Point mode
 */
esp_err_t wifi_init_softap(void);

/**
 * @brief Scan for available networks
 * 
 * @param ap_info Pointer to array of wifi_ap_record_t
 * @param ap_count Pointer to store number of networks found
 */
esp_err_t wifi_scan_networks(wifi_ap_record_t *ap_info, uint16_t *ap_count);

/**
 * @brief Connect to a WiFi network
 * 
 * @param ssid Network SSID
 * @param password Network password
 */
esp_err_t wifi_connect_sta(const char *ssid, const char *password);

/**
 * @brief Get current WiFi connection status
 * 
 * @return true if connected, false otherwise
 */
bool wifi_is_connected(void);

/**
 * @brief Get current IP address
 * 
 * @param ip_addr Buffer to store IP address (minimum 16 bytes)
 * @return true if IP is available, false otherwise
 */
bool wifi_get_ip_address(char *ip_addr);


/**
 * @brief Handles Wi-Fi events.
 *
 * This function is a callback for handling various Wi-Fi events. It is typically
 * registered with the ESP-IDF event loop to process events related to Wi-Fi.
 *
 * @param arg User-defined argument passed to the event handler.
 * @param event_base The base identifier of the event (e.g., WIFI_EVENT).
 * @param event_id The specific ID of the event within the event base.
 * @param event_data Pointer to the event-specific data, or NULL if no data is provided.
 */
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H;