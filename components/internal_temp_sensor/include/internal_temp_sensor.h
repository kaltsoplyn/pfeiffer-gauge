#ifndef INTERNAL_TEMP_SENSOR_H
#define INTERNAL_TEMP_SENSOR_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the internal temperature sensor.
 *
 * Installs and enables the temperature sensor driver.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t internal_temp_sensor_init(void);

/**
 * @brief Read the internal temperature in Celsius.
 *
 * @param[out] celsius Pointer to a float where the temperature reading will be stored.
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized, or other error code on read failure.
 */
esp_err_t internal_temp_sensor_read(float *celsius);

#ifdef __cplusplus
}
#endif

#endif // INTERNAL_TEMP_SENSOR_H