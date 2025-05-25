#pragma once

#include "app_manager.h"
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

/**
 * @brief Deinitializes the network component.
 *
 * This function releases any resources allocated by the network component and
 * performs necessary cleanup operations. After calling this function, the network
 * component should not be used unless it is reinitialized.
 *
 * @return
 *     - ESP_OK on successful deinitialization
 *     - Appropriate error code from esp_err_t on failure
 */
esp_err_t network_comp_deinit(void);

/**
 * @brief Toggles the state of the web server.
 *
 * This function starts the web server if it is currently stopped,
 * or stops it if it is currently running.
 *
 * @return
 *     - ESP_OK on success
 *     - Appropriate error code from esp_err_t on failure
 */
esp_err_t network_comp_toggle_web_server(void);


#ifdef __cplusplus
}
#endif