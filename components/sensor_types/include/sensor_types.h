#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_adc/adc_oneshot.h"

// --- Sensor Types ---
typedef struct {
    float pressure;
    uint64_t timestamp;
} PressureData;

typedef struct {
    float temperature;
    uint64_t timestamp;
} TemperatureData;

typedef struct {
    PressureData pressure_data;
    TemperatureData temperature_data;
    TemperatureData internal_temp_data;
} SensorData_t;


// --- Default Configuration Values ---
#define DEFAULT_SAMPLING_INTERVAL_MS            50
#define DEFAULT_DISPLAY_UPDATE_INTERVAL_MS    1000
#define DEFAULT_PRESSURE_GAUGE_FS               100.0f
#define DEFAULT_MOCK_MODE                       false
#define DATA_BUFFER_SIZE                        500


/**
 * @brief Retrieve the ADC unit handle for sensor types.
 *
 * This function provides access to the ADC (Analog-to-Digital Converter) unit handle
 * used for sensor type operations. The handle can be used to perform ADC-related
 * tasks such as reading analog values from sensors.
 *
 * @return adc_oneshot_unit_handle_t The handle to the ADC unit.
 */
adc_oneshot_unit_handle_t sensor_types_get_adc_unit_handle();

/**
 * @brief Initialize the ADC (Analog-to-Digital Converter).
 * 
 * This function sets up the ADC1 unit for operation. It configures the ADC 
 * according to the application requirements and prepares it for reading 
 * analog signals. The function returns an esp_err_t indicating the success 
 * or failure of the initialization process.
 * 
 * @return 
 *      - ESP_OK: Initialization was successful.
 *      - Other error codes: An error occurred during initialization.
 */
esp_err_t adc_init();