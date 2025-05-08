#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "sensor_types.h"

#define DATA_BUFFER_SIZE 500
#define PRESSURE_GAUGE_FS    100.0f // pressure gauge full scale in mbar | for Pfeiffer CMR362, F.S. = 100mbar | put in header for possible use in the GUI


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initializes the pressure measurement component.
 *
 * This function sets up the necessary resources and configurations
 * required for the pressure measurement component to operate.
 *
 * @return
 *     - ESP_OK: Initialization was successful.
 *     - Appropriate error code from esp_err_t if initialization fails.
 */
esp_err_t pressure_meas_init();

/**
 * @brief Reads raw pressure data from the sensor.
 *
 * This function retrieves the raw pressure data from the connected
 * pressure measurement sensor. The returned data is not processed
 * or calibrated and represents the direct output from the sensor.
 *
 * @return PressureData The raw pressure data structure containing
 *         the sensor's output values.
 */
PressureData pressure_meas_read_raw();

/**
 * @brief Retrieves the latest pressure measurement data.
 *
 * This function returns the most recent pressure data collected
 * by the pressure measurement component.
 *
 * @return PressureData The latest pressure measurement data.
 */
PressureData pressure_meas_get_latest_data(void);

/**
 * @brief Updates the latest pressure measurement data.
 *
 * This function is used to update the most recent pressure measurement
 * data with the provided `PressureData` structure.
 *
 * @param PressureData The new pressure measurement data to be updated.
 */
void pressure_meas_update_latest_data(PressureData);

// Get buffered pressure data since last call and reset buffer
/**
 * @brief Copies buffered pressure data into the provided array and resets the buffer.
 *
 * @param[out] out_buffer Pointer to an array of PressureData where measurements will be copied.
 * @param[in] max_count The maximum number of elements the out_buffer can hold.
 * @return The number of measurements actually copied into out_buffer.
 */
int pressure_meas_get_buffered_data(PressureData *out_buffer, int max_count);

/**
 * @brief Retrieves the percentage of how full the pressure measurement buffer is.
 *
 * This function calculates and returns the current fill level of the buffer
 * used for storing pressure measurement data as a percentage.
 *
 * @return int The buffer fill percentage (0-100).
 */
int pressure_meas_get_buffer_full_percentage();


/**
 * @brief Retrieves the pressure measurement data in JSON format.
 *
 * This function returns a pointer to a buffer containing the pressure
 * measurement data formatted as a JSON string. The caller is responsible
 * for ensuring the buffer is properly handled and freed if necessary.
 *
 * @return A pointer to a null-terminated string containing the JSON-formatted
 *         temperature measurement data. The ownership and lifetime of the
 *         buffer depend on the implementation.
 */
char* pressure_meas_get_data_buffer_json();

#ifdef __cplusplus
}
#endif