#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "sensor_types.h"

#define TEMPERATURE_SENSOR_NAME "10k Thermistor"

#ifdef __cplusplus
extern "C" {
#endif



/**
 * @brief Initializes the temperature measurement component.
 *
 * This function sets up the necessary resources and configurations
 * required for the temperature measurement component to operate.
 *
 * @return
 *     - ESP_OK: Initialization was successful.
 *     - Appropriate error code from esp_err_t if initialization fails.
 */
esp_err_t temp_meas_init();

/**
 * @brief Reads raw temperature data from the sensor.
 *
 * This function retrieves the raw temperature data from the connected
 * temperature measurement sensor. The returned data is not processed
 * or calibrated and represents the direct output from the sensor.
 *
 * @return TemperatureData The raw temperature data structure containing
 *         the sensor's output values.
 */
TemperatureData temp_meas_read_raw();

/**
 * @brief Retrieves the latest temperature measurement data.
 *
 * This function returns the most recent temperature data collected
 * by the temperature measurement component.
 *
 * @return TemperatureData The latest temperature measurement data.
 */
TemperatureData temp_meas_get_latest_data(void);

/**
 * @brief Updates the latest temperature measurement data.
 *
 * This function is used to update the most recent temperature measurement
 * data with the provided `TemperatureData` structure.
 *
 * @param TemperatureData The new temperature measurement data to be updated.
 */
void temp_meas_update_latest_data(TemperatureData);

/**
 * @brief Copies buffered temperature data into the provided array and resets the buffer.
 *
 * @param[out] out_buffer Pointer to an array of TemperatureData where measurements will be copied.
 * @param[in] max_count The maximum number of elements the out_buffer can hold.
 * @return The number of measurements actually copied into out_buffer.
 */
int temp_meas_get_buffered_data(TemperatureData *out_buffer, int max_count);

/**
 * @brief Retrieves the percentage of how full the temperature measurement buffer is.
 *
 * This function calculates and returns the current fill level of the buffer
 * used for storing temperature measurement data as a percentage.
 *
 * @return int The buffer fill percentage (0-100).
 */
int temp_meas_get_buffer_full_percentage();


/**
 * @brief Retrieves the temperature measurement data in JSON format.
 *
 * This function provides access to a buffer containing temperature
 * measurement data formatted as a JSON string. The caller is responsible
 * for handling the returned buffer appropriately.
 *
 * @return A pointer to a null-terminated string containing the JSON-formatted
 *         temperature measurement data. The ownership and lifetime of the
 *         buffer depend on the implementation.
 */
char* temp_meas_get_data_buffer_json();

#ifdef __cplusplus
}
#endif