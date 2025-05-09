#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include "sensor_types.h"



#ifdef __cplusplus
extern "C" {
#endif



// --- Application Configuration ---
typedef struct {
    int sampling_interval_ms;           // Default: 50, Min: 5
    int display_update_interval_ms;     // Default: 1000, Min: 100
    int data_buffer_size;               // Default: 500, Max: 1000 (for SensorData_t array)
    float pressure_gauge_FS;            // Default: 100.0 (Full Scale in mbar)
    bool mock_mode;                     // Default: false
    adc_oneshot_unit_handle_t adc_unit_handle; 
} AppConfig_t;

// --- Application Runtime State (internal to app_manager.c) ---
// AppState_t will be static in app_manager.c
// We expose accessors for its members.

// --- Initialization ---
/**
 * @brief Initialize the Application Manager.
 * Loads default configurations and initializes state.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t app_manager_init(void);

// --- Configuration Accessors ---
/**
 * @brief Get a copy of the current application configuration.
 * @param[out] config Pointer to an AppConfig_t struct to copy the config into.
 */
void app_manager_get_config(AppConfig_t *config_out); // Renamed param for clarity

int app_manager_get_sampling_interval_ms(void);
esp_err_t app_manager_set_sampling_interval_ms(int interval_ms);

int app_manager_get_display_update_interval_ms(void);
esp_err_t app_manager_set_display_update_interval_ms(int interval_ms);

int app_manager_get_data_buffer_size(void);
// Note: Setting data_buffer_size at runtime would require reallocating the buffer.
// For simplicity, this might be a read-only config after init, or setter needs careful implementation.
// For now, we'll make it settable but it won't resize the actual buffer post-init.
esp_err_t app_manager_set_data_buffer_size(int size);

float app_manager_get_pressure_gauge_FS(void);
esp_err_t app_manager_set_pressure_gauge_FS(float fs);

bool app_manager_get_mock_mode(void);
void app_manager_set_mock_mode(bool enable);

// --- State Accessors/Modifiers (Thread-Safe) ---

uint64_t app_manager_get_start_time_ms(void);
// start_time_ms is set once at init, no public setter.

bool app_manager_get_sampling_active(void);
void app_manager_set_sampling_active(bool active);

/**
 * @brief Sets the desired Wi-Fi active state.
 * This function will attempt to start or stop Wi-Fi accordingly.
 * @param active True to activate Wi-Fi, false to deactivate.
 * @return ESP_OK if the command to change state was successfully initiated,
 *         or an error code if the underlying Wi-Fi operation failed.
 */
esp_err_t app_manager_set_wifi_active(bool active);
/**
 * @brief Gets the current operational status of Wi-Fi.
 * Checks if the STA interface is connected or if the AP is active.
 * @return True if Wi-Fi (STA connected or AP active) is operational, false otherwise.
 */
bool app_manager_get_wifi_status(void); // Renamed from get_wifi_active for clarity

float app_manager_get_internal_temp_c(void);
void app_manager_update_internal_temp_c(float temp_c); // Called by the temp sensor task

SensorData_t app_manager_get_latest_sensor_data(void);
void app_manager_update_latest_sensor_data(SensorData_t data); // Called by the main measurement task


/**
 * @brief Retrieves the JSON data buffer managed by the application manager.
 *
 * This function provides access to a buffer containing JSON-formatted data
 * used within the application. The caller is responsible for ensuring the
 * buffer is used appropriately and not modified unless explicitly allowed.
 *
 * @return A pointer to the JSON data buffer as a null-terminated string.
 *         The returned pointer must not be freed by the caller.
 */
char* app_manager_get_data_buffer_json();

// /**
//  * @brief Adds a new sensor data point to the internal circular buffer.
//  * @param data The SensorData_t to add.
//  */
// void app_manager_add_sensor_data_to_buffer(SensorData_t data);

// /**
//  * @brief Copies buffered sensor data into the provided array and resets the read pointer for this data.
//  *
//  * @param[out] out_buffer Pointer to an array of SensorData_t where measurements will be copied.
//  * @param[in] max_out_count The maximum number of elements the out_buffer can hold.
//  * @return The number of measurements actually copied into out_buffer.
//  */
// int app_manager_get_buffered_sensor_data(SensorData_t *out_buffer, int max_out_count);

//int app_manager_get_sensor_buffer_fill_percentage(void);

#ifdef __cplusplus
}
#endif