#include "sensor_types.h"
#include "esp_log.h"

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
