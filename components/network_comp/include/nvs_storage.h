#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize NVS storage system
 */
esp_err_t nvs_init(void);

/**
 * @brief Store WiFi credentials in NVS
 * 
 * @param ssid Network SSID
 * @param password Network password
 */
esp_err_t nvs_store_wifi_creds(const char *ssid, const char *password);

/**
 * @brief Retrieve stored WiFi credentials
 * 
 * @param ssid Buffer to store SSID (minimum 32 bytes)
 * @param ssid_len Length of SSID buffer
 * @param password Buffer to store password (minimum 64 bytes)
 * @param password_len Length of password buffer
 * @return true if credentials were found, false otherwise
 */
bool nvs_get_wifi_creds(char *ssid, size_t ssid_len, char *password, size_t password_len);

/**
 * @brief Erase stored WiFi credentials
 */
esp_err_t nvs_erase_wifi_creds(void);

#ifdef __cplusplus
}
#endif

#endif // NVS_STORAGE_H