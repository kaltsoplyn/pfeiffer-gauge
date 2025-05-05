#include "esp_log.h"
#include "driver/temperature_sensor.h"
#include "internal_temp_sensor.h"

static const char *TAG_TEMP = "TempSensor";
static temperature_sensor_handle_t s_temp_handle = NULL; // Static handle for the sensor

esp_err_t internal_temp_sensor_init(void) {
    if (s_temp_handle != NULL) {
        ESP_LOGW(TAG_TEMP, "Temperature sensor already initialized");
        return ESP_OK; // Already initialized
    }

    ESP_LOGI(TAG_TEMP, "Initializing temperature sensor");
    // Initialize the temperature sensor
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50); // Set range for ESP32-C6, check datasheet/TRM for optimal values
    esp_err_t err = temperature_sensor_install(&temp_sensor_config, &s_temp_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_TEMP, "Failed to install temperature sensor: %s", esp_err_to_name(err));
        s_temp_handle = NULL; // Ensure handle is NULL on failure
        return err;
    }

    // Enable the sensor
    err = temperature_sensor_enable(s_temp_handle);
     if (err != ESP_OK) {
        ESP_LOGE(TAG_TEMP, "Failed to enable temperature sensor: %s", esp_err_to_name(err));
        temperature_sensor_uninstall(s_temp_handle); // Clean up on error
        s_temp_handle = NULL; // Ensure handle is NULL on failure
        return err;
    }

    ESP_LOGI(TAG_TEMP, "Temperature sensor initialized and enabled.");
    return ESP_OK;
}

esp_err_t internal_temp_sensor_read(float *celsius) {
    if (s_temp_handle == NULL) {
        ESP_LOGE(TAG_TEMP, "Temperature sensor not initialized.");
        return ESP_ERR_INVALID_STATE; // Indicate that init needs to be called first
    }
    if (celsius == NULL) {
        return ESP_ERR_INVALID_ARG; // Check for null pointer
    }

    // Read the temperature
    esp_err_t err = temperature_sensor_get_celsius(s_temp_handle, celsius);
    if (err == ESP_OK) {
        ESP_LOGD(TAG_TEMP, "Internal Temperature: %.2f °C", *celsius); // Use Debug level for periodic reads
    } else {
        ESP_LOGE(TAG_TEMP, "Failed to read temperature: %s", esp_err_to_name(err));
    }
    return err;
}
