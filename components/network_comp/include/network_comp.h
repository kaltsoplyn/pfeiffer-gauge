#ifndef NETWORK_COMP_H
#define NETWORK_COMP_H

#include <stdbool.h>
#include "esp_err.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "nvs_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the network configuration component
 * 
 * This function initializes all necessary subsystems including:
 * - NVS storage
 * - WiFi (either STA or AP mode depending on stored credentials)
 * - Web server (either config or data mode)
 * - Reset button GPIO
 */
esp_err_t network_comp_init(void);



// Configuration constants
// the wifi-related are in the wifi_manager.h
#define CONFIG_BUTTON_GPIO     5       // Default GPIO for reset button
#define CONFIG_BUTTON_PRESS_MS 3000    // Long press duration in milliseconds

#ifdef __cplusplus
}
#endif

#endif // NETWORK_COMP_H