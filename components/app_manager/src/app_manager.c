#include "app_manager.h"
#include "esp_log.h"
#include "esp_timer.h" // For esp_timer_get_time
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "wifi_manager.h" // For app_manager_set_wifi_active and get_wifi_status
#include <string.h>

#include "pressure_meas_comp.h"
#include "temp_meas_comp.h"
#include "internal_temp_sensor.h"

// --- Default Configuration Values ---
#define DEFAULT_SAMPLING_INTERVAL_MS            50
#define DEFAULT_DISPLAY_UPDATE_INTERVAL_MS    1000
#define DEFAULT_PRESSURE_GAUGE_FS               100.0f
#define DEFAULT_MOCK_MODE                       false

static const char *TAG = "AppManager";

static AppConfig_t s_app_config;

typedef struct {
    uint64_t start_time_ms;
    bool sampling_active;
    bool desired_wifi_active; // Tracks the requested state
    float internal_temp;
    SensorData_t latest_sensor_data;
} AppState_t;

static AppState_t s_app_state;
static SemaphoreHandle_t s_config_mutex = NULL; // For thread-safe access to config
static SemaphoreHandle_t s_state_mutex = NULL;  // For thread-safe access to runtime state

// --- Internal Sensor Data Buffer ---
static SensorData_t *s_sensor_data_buffer = NULL;
static volatile int s_buffer_write_idx = 0;
static volatile int s_buffer_read_idx = 0;
static volatile bool s_buffer_full = false;
static int s_actual_buffer_size = 0; // To store the size set at init or by setter

esp_err_t app_manager_init(void) {
    s_config_mutex = xSemaphoreCreateMutex();
    s_state_mutex = xSemaphoreCreateMutex();

    if (!s_config_mutex || !s_state_mutex) {
        ESP_LOGE(TAG, "Failed to create mutexes!");
        if(s_config_mutex) vSemaphoreDelete(s_config_mutex);
        if(s_state_mutex) vSemaphoreDelete(s_state_mutex);
        return ESP_FAIL;
    }

    // Initialize with default config values
    s_app_config.sampling_interval_ms = DEFAULT_SAMPLING_INTERVAL_MS;
    s_app_config.display_update_interval_ms = DEFAULT_DISPLAY_UPDATE_INTERVAL_MS;
    s_app_config.data_buffer_size = DATA_BUFFER_SIZE;
    s_app_config.pressure_gauge_FS = DEFAULT_PRESSURE_GAUGE_FS;
    s_app_config.mock_mode = DEFAULT_MOCK_MODE;

    s_actual_buffer_size = s_app_config.data_buffer_size;
    s_sensor_data_buffer = (SensorData_t*)malloc(s_actual_buffer_size * sizeof(SensorData_t));
    if (!s_sensor_data_buffer) {
        ESP_LOGE(TAG, "Failed to allocate sensor data buffer (%d entries)!", s_actual_buffer_size);
        vSemaphoreDelete(s_config_mutex);
        vSemaphoreDelete(s_state_mutex);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Sensor data buffer allocated for %d entries.", s_actual_buffer_size);

    // Initialize default state values
    s_app_state.start_time_ms = esp_timer_get_time() / 1000;
    s_app_state.sampling_active = true;
    s_app_state.desired_wifi_active = true; // Assume Wi-Fi should be active on boot
    s_app_state.internal_temp = -273.15f; // Invalid initial temp
    s_app_state.latest_sensor_data = (SensorData_t){
        .pressure_data = {.pressure = -1.0f, .timestamp = 0},
        .temperature_data = {.temperature = -273.15f, .timestamp = 0},
        .internal_temp_data = {.temperature = -273.15f, .timestamp = 0}
    };

    ESP_LOGI(TAG, "Application Manager initialized. Start time: %llu ms", s_app_state.start_time_ms);
    return ESP_OK;
}

// --- Configuration ---
void app_manager_get_config(AppConfig_t *config_out) {
    if (config_out && xSemaphoreTake(s_config_mutex, portMAX_DELAY) == pdTRUE) {
        *config_out = s_app_config;
        xSemaphoreGive(s_config_mutex);
    }
}

int app_manager_get_sampling_interval_ms(void) { return s_app_config.sampling_interval_ms; }
esp_err_t app_manager_set_sampling_interval_ms(int interval_ms) {
    if (interval_ms < 5) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) == pdTRUE) {
        s_app_config.sampling_interval_ms = interval_ms;
        xSemaphoreGive(s_config_mutex);
        ESP_LOGI(TAG, "Config: Sampling interval set to %d ms", interval_ms);
        return ESP_OK;
    }
    return ESP_FAIL;
}

int app_manager_get_display_update_interval_ms(void) { return s_app_config.display_update_interval_ms; }
esp_err_t app_manager_set_display_update_interval_ms(int interval_ms) {
    if (interval_ms < 100) return ESP_ERR_INVALID_ARG;
     if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) == pdTRUE) {
        s_app_config.display_update_interval_ms = interval_ms;
        xSemaphoreGive(s_config_mutex);
        ESP_LOGI(TAG, "Config: Display update interval set to %d ms", interval_ms);
        return ESP_OK;
    }
    return ESP_FAIL;
}

int app_manager_get_data_buffer_size(void) { return s_app_config.data_buffer_size; }
esp_err_t app_manager_set_data_buffer_size(int size) {
    if (size <= 0 || size > 1000) return ESP_ERR_INVALID_ARG;
    // Note: This currently only changes the config value.
    // Resizing s_sensor_data_buffer at runtime is complex and not implemented here.
    // It would require freeing the old buffer, allocating new, and handling data migration/loss.
    // For now, the actual buffer size is fixed at init based on the initial config.
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) == pdTRUE) {
        s_app_config.data_buffer_size = size;
        // s_actual_buffer_size remains unchanged after init in this simplified version
        xSemaphoreGive(s_config_mutex);
        ESP_LOGI(TAG, "Config: Data buffer size set to %d (actual buffer size fixed at init: %d)", size, s_actual_buffer_size);
        return ESP_OK;
    }
    return ESP_FAIL;
}

float app_manager_get_pressure_gauge_FS(void) { return s_app_config.pressure_gauge_FS; }
esp_err_t app_manager_set_pressure_gauge_FS(float fs) {
    if (fs <= 0) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) == pdTRUE) {
        s_app_config.pressure_gauge_FS = fs;
        xSemaphoreGive(s_config_mutex);
        ESP_LOGI(TAG, "Config: Pressure Gauge FS set to %.2f", fs);
        return ESP_OK;
    }
    return ESP_FAIL;
}

bool app_manager_get_mock_mode(void) { return s_app_config.mock_mode; }
void app_manager_set_mock_mode(bool enable) {
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) == pdTRUE) {
        s_app_config.mock_mode = enable;
        xSemaphoreGive(s_config_mutex);
        ESP_LOGI(TAG, "Config: Mock mode set to %s", enable ? "true" : "false");
    }
}

// --- State ---
uint64_t app_manager_get_start_time_ms(void) { return s_app_state.start_time_ms; }

bool app_manager_get_sampling_active(void) {
    bool active = false;
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        active = s_app_state.sampling_active;
        xSemaphoreGive(s_state_mutex);
    }
    return active;
}

void app_manager_set_sampling_active(bool active) {
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        s_app_state.sampling_active = active;
        xSemaphoreGive(s_state_mutex);
        ESP_LOGI(TAG, "State: Sampling active set to: %s", active ? "true" : "false");
    }
}

esp_err_t app_manager_set_wifi_active(bool active) {
    esp_err_t ret = ESP_FAIL;
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        s_app_state.desired_wifi_active = active;
        xSemaphoreGive(s_state_mutex);
    }
    // Delegate to wifi_manager (these functions would need to be exposed from wifi_manager.h)
    if (active) {
        ESP_LOGI(TAG, "Requesting Wi-Fi START");
        ret = wifi_start(); // Assumes wifi_manager_start() handles logic like network_comp_attempt_wifi_connection
    } else {
        ESP_LOGI(TAG, "Requesting Wi-Fi STOP");
        ret = wifi_stop();  // Assumes wifi_manager_stop() calls esp_wifi_stop()
    }
    return ret;
}

bool app_manager_get_wifi_status(void) {
    return wifi_is_connected(); // Directly use wifi_manager's status check
}

float app_manager_get_internal_temp(void) { 
    float internal_temp;
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        internal_temp = s_app_state.internal_temp;
        xSemaphoreGive(s_state_mutex);
        return internal_temp;
    }
    /* ... (same as before, with mutex) ... */ return s_app_state.internal_temp; } // Simplified for brevity
void app_manager_update_internal_temp(float temp_c) {
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        s_app_state.internal_temp = temp_c;
        xSemaphoreGive(s_state_mutex);
    }  
}

SensorData_t app_manager_get_latest_sensor_data(void) {
    SensorData_t latest_sensor_data;
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        latest_sensor_data = s_app_state.latest_sensor_data;
        xSemaphoreGive(s_state_mutex);
    }
    return latest_sensor_data;
}
void app_manager_update_latest_sensor_data(SensorData_t data) { 
    if (xSemaphoreTake(s_state_mutex, portMAX_DELAY) == pdTRUE) {
        s_app_state.latest_sensor_data = data;
        xSemaphoreGive(s_state_mutex);
    }
}


char* app_manager_get_data_buffer_json() {
    char *pressure_json = pressure_meas_get_data_buffer_json();
    char *temperature_json = temp_meas_get_data_buffer_json();
    char *internal_temp_json = internal_temp_sensor_get_data_buffer_json();

    if (pressure_json == NULL || temperature_json == NULL || internal_temp_json == NULL) {
        ESP_LOGE(TAG, "Failed to get JSON data for pressure or temperature. P: %p, T: %p, Internal T: %p", pressure_json, temperature_json, internal_temp_json);
        // Free whichever one might have been allocated
        if (pressure_json) free(pressure_json);
        if (temperature_json) free(temperature_json);
        if (internal_temp_json) free(internal_temp_json);
        
        return NULL;
    }

    // Ensure null termination for safety, though strlen should handle it.
    // pressure_json[strlen(pressure_json)] = '\0'; 
    // temperature_json[strlen(temperature_json)] = '\0';
    int total_sensor_json_len = strlen(pressure_json) + strlen(temperature_json) + strlen(internal_temp_json);
    int total_len = total_sensor_json_len + 64;
    int written;
    char *combined_sensor_json = malloc(total_len);
    written = snprintf(combined_sensor_json, total_len, "{\"pressure\":%s,\"temperature\":%s,\"internal_temp\":%s}", pressure_json, temperature_json, internal_temp_json);
    if (written >= total_len) {
        ESP_LOGW(TAG, "JSON buffer potentially truncated");
        // Handle truncation: send error or ensure buffer is always sufficient
        free(pressure_json);
        free(temperature_json);
        free(internal_temp_json);
        free(combined_sensor_json);
        return NULL;
    }

    free(pressure_json);
    free(temperature_json);
    free(internal_temp_json);
    return combined_sensor_json;
}

// void app_manager_add_sensor_data_to_buffer(SensorData_t data) {
//     if (!s_sensor_data_buffer) return;
//     if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
//         s_sensor_data_buffer[s_buffer_write_idx] = data;
//         s_buffer_write_idx = (s_buffer_write_idx + 1) % s_actual_buffer_size;
//         if (s_buffer_full && s_buffer_write_idx == s_buffer_read_idx) {
//             s_buffer_read_idx = (s_buffer_read_idx + 1) % s_actual_buffer_size;
//         } else if (s_buffer_write_idx == s_buffer_read_idx) {
//             s_buffer_full = true;
//         }
//         xSemaphoreGive(s_state_mutex);
//     } else {
//         ESP_LOGE(TAG, "Failed to take state_mutex in add_sensor_data_to_buffer");
//     }
// }

// int app_manager_get_buffered_sensor_data(SensorData_t *out_buffer, int max_out_count) {
//     if (!s_sensor_data_buffer || !out_buffer || max_out_count <= 0) return 0;
//     int count = 0;
//     if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
//         int current_write_idx = s_buffer_write_idx;
//         bool was_buffer_full = s_buffer_full;
//         int temp_read_idx = s_buffer_read_idx;

//         while (count < max_out_count) {
//             if (!was_buffer_full && temp_read_idx == current_write_idx) break;
//             if (was_buffer_full && count >= s_actual_buffer_size) break;

//             out_buffer[count++] = s_sensor_data_buffer[temp_read_idx];
//             temp_read_idx = (temp_read_idx + 1) % s_actual_buffer_size;

//             if (was_buffer_full && temp_read_idx == current_write_idx) break;
//         }
//         s_buffer_read_idx = temp_read_idx;
//         s_buffer_full = false;
//         xSemaphoreGive(s_state_mutex);
//     } else {
//         ESP_LOGE(TAG, "Failed to take state_mutex in get_buffered_sensor_data");
//     }
//     return count;
// }

// int app_manager_get_sensor_buffer_fill_percentage(void) {
//     int percentage = -1;
//     if (!s_sensor_data_buffer) return -1;
//     if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
//         int count;
//         count = (s_buffer_write_idx - s_buffer_read_idx + s_actual_buffer_size) % s_actual_buffer_size;
//         if (s_buffer_full && count == 0) {
//             count = s_actual_buffer_size;
//         }
//         percentage = (s_actual_buffer_size > 0) ? ((count * 100) / s_actual_buffer_size) : 0;
//         xSemaphoreGive(s_state_mutex);
//     } else {
//         ESP_LOGE(TAG, "Failed to take state_mutex in get_sensor_buffer_fill_percentage");
//     }
//     return percentage;
// }