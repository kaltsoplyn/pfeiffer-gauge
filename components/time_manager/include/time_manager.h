#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include <time.h>       // For time_t
#include <sys/time.h>   // For struct timeval

#define POSIX_TIMEZONE_STRING   "MSK-3"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type for time synchronization notifications.
 */
typedef void (*time_sync_user_cb_t)(void);

/**
 * @brief Initializes the Time Manager component.
 * Sets up SNTP to synchronize with a time server. This should ideally be called
 * after the network (Wi-Fi) is connected, though SNTP will retry if the network
 * is not yet available.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t time_manager_init(void);

/**
 * @brief Deinitializes the Time Manager component.
 * Stops the SNTP service.
 */
void time_manager_deinit(void);

/**
 * @brief Gets the current Unix timestamp.
 *
 * @return time_t The current time as seconds since the Unix epoch.
 *         Returns 0 if time is not yet synchronized or an error occurs.
 */
time_t time_manager_get_timestamp_s(void);

/**
 * @brief Gets the current Unix timestamp.
 *
 * @return time_t The current time as milliseconds since the Unix epoch.
 *         Returns 0 if time is not yet synchronized or an error occurs.
 */
time_t time_manager_get_timestamp_ms(void);

/**
 * @brief Gets the current time as a struct timeval (seconds and microseconds).
 *
 * @param[out] tv Pointer to a struct timeval to store the current time.
 * @return ESP_OK if time is available and successfully retrieved,
 *         ESP_ERR_INVALID_STATE if not synchronized,
 *         ESP_ERR_INVALID_ARG if tv is NULL.
 */
esp_err_t time_manager_get_timeval(struct timeval *tv);

/**
 * @brief Checks if the system time is synchronized with an NTP server.
 *
 * @return true if time is synchronized, false otherwise.
 */
bool time_manager_is_synced(void);

/**
 * @brief Registers a user callback function to be invoked upon successful time synchronization.
 *
 * @param cb The callback function to register. Pass NULL to unregister.
 */
void time_manager_register_sync_callback(time_sync_user_cb_t cb);

#ifdef __cplusplus
}
#endif
