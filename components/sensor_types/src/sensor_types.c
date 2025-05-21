#include "sensor_types.h"
#include "esp_log.h"
#include "esp_timer.h"

static char *TAG = "SensorTypes";

adc_oneshot_unit_handle_t adc1_handle = NULL;

esp_err_t adc_init() {
    // Initialize ADC_UNIT_1
    adc_oneshot_unit_init_cfg_t adc1_init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE, // Or ADC_ULP_MODE_FSM if using ULP
    };
    esp_err_t adc_init_ret = adc_oneshot_new_unit(&adc1_init_config, &adc1_handle);
    if (adc_init_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC_UNIT_1: %s", esp_err_to_name(adc_init_ret));
        return adc_init_ret;
    }

    ESP_LOGI(TAG, "ADC_UNIT_1 initialized");
    return adc_init_ret;
}

adc_oneshot_unit_handle_t sensor_types_get_adc_unit_handle() {
    return adc1_handle;
}

// uint64_t sensor_types_get_timestamp_ms() {
//     uint64_t timestamp_ms = 0; // Default to 0 if epoch time is not available
//     struct timeval current_time_tv;

//     if (time_manager_get_timeval(&current_time_tv) == ESP_OK) {
//         timestamp_ms = ((uint64_t)current_time_tv.tv_sec * 1000ULL) + (current_time_tv.tv_usec / 1000ULL);
//     } else {
//         timestamp_ms = esp_timer_get_time() / 1000; // Time since boot in ms
//         //ESP_LOGW(TAG, "Epoch time not available, using time since boot for timestamp.");
//     }

//     return timestamp_ms;
// }