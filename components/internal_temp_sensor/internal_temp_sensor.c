/*
 * Component for reading the SoC internal temperature
*/

#include "internal_temp_sensor.h"
#include "freertos/FreeRTOS.h" // Required for mutex
#include "freertos/semphr.h"  // Required for mutex
#include "esp_log.h"
#include "driver/temperature_sensor.h"
#include "internal_temp_sensor.h"
#include "esp_timer.h"
#include "time.h"
#include "time_manager.h"

#define MOCK    false

// ### TODO : make into a sensor, like the pressure and temperature

static const char *TAG = "TempSensor";


// --- Circular Buffer ---
static TemperatureData temp_buffer[DATA_BUFFER_SIZE];
static volatile int buffer_write_idx = 0; // Index where next measurement will be written
static volatile int buffer_read_idx = 0;  // Index where the API last read up to
static volatile bool buffer_full = false; // Flag to indicate if buffer has wrapped around

// --- State Variables ---
static SemaphoreHandle_t s_int_temp_mutex = NULL;
static TemperatureData s_current_int_temp_state = {.temperature = -273.15f, .timestamp = 0}; // Initialize to invalid

static temperature_sensor_handle_t s_int_temp_handle = NULL; // Static handle for the sensor

// --- Mock Data ---
static int mock_previous_adc;
#if MOCK
mock_previous_adc = 2000;
#endif

esp_err_t internal_temp_sensor_init() {
    if (MOCK) {
        ESP_LOGI(TAG, "Initializing internal temperature measurement in MOCK mode.");
        srand(time(NULL)); // MOCK
        // Initialize mutex even in mock mode if getter/setter functions might be called
        s_int_temp_mutex = xSemaphoreCreateMutex();
        return (s_int_temp_mutex == NULL) ? ESP_FAIL : ESP_OK; // Return status based on mutex creation
    }

    if (s_int_temp_handle != NULL) {
        ESP_LOGW(TAG, "Temperature sensor already initialized");
        return ESP_OK; // Already initialized
    }

    ESP_LOGI(TAG, "Initializing internal temperature sensor");
    // Initialize the temperature sensor
    temperature_sensor_config_t int_temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50); // Set range for ESP32-C6, check datasheet/TRM for optimal values
    esp_err_t err = temperature_sensor_install(&int_temp_sensor_config, &s_int_temp_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install internal temperature sensor: %s", esp_err_to_name(err));
        s_int_temp_handle = NULL; // Ensure handle is NULL on failure
        return err;
    }

    // Enable the sensor
    err = temperature_sensor_enable(s_int_temp_handle);
     if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable temperature sensor: %s", esp_err_to_name(err));
        temperature_sensor_uninstall(s_int_temp_handle); // Clean up on error
        s_int_temp_handle = NULL; // Ensure handle is NULL on failure
        return err;
    }

    // Initialize mutex
    s_int_temp_mutex = xSemaphoreCreateMutex();
    if (s_int_temp_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create internal temperature mutex!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Internal temperature sensor initialized and enabled.");
    return ESP_OK;
}

esp_err_t internal_temp_sensor_read(float *celsius) {
    if (s_int_temp_handle == NULL) {
        ESP_LOGE(TAG, "Internal temperature sensor not initialized.");
        return ESP_ERR_INVALID_STATE;
    }
    if (celsius == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Read the temperature
    esp_err_t err = temperature_sensor_get_celsius(s_int_temp_handle, celsius);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read internal temperature: %s", esp_err_to_name(err));
    }
    return err;
}

// Reads ADC, converts to temperature, and returns the raw data point
TemperatureData internal_temp_sensor_read_raw() {
    float temperature = -273.15;
    float* temp_ptr = &temperature; 
    internal_temp_sensor_read(temp_ptr);
    uint64_t timestamp = time_manager_get_timestamp_ms();  //esp_timer_get_time() / 1000;

    TemperatureData current_measurement = {temperature, timestamp};

    // --- Store in circular buffer - Use mutex for shared state variables ---
    if (xSemaphoreTake(s_int_temp_mutex, pdMS_TO_TICKS(10)) == pdTRUE) { // Use short timeout
    temp_buffer[buffer_write_idx] = current_measurement;
    buffer_write_idx = (buffer_write_idx + 1) % DATA_BUFFER_SIZE;

    if (buffer_full && buffer_write_idx == buffer_read_idx) {
        // Overwriting data that hasn't been read yet, advance read pointer
        buffer_read_idx = (buffer_read_idx + 1) % DATA_BUFFER_SIZE;
        // buffer_full remains true
    } else if (buffer_write_idx == buffer_read_idx) {
        buffer_full = true; // Buffer just became full or wrapped exactly
    }

        xSemaphoreGive(s_int_temp_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take mutex in int_temp_meas_read_raw - data point lost");
        // Handle the case where data couldn't be written (e.g., skip this point)
    }
    // --- End Store ---

    // if (temperature < 0) {
    //     ESP_LOGW(TAG, "Temperature's getting a bit low, don't you think? T = %.1f°C", temperature);
    // } else if (temperature > 50) {
    //     ESP_LOGW(TAG, "Temperature's getting a bit high: %.1f°C", temperature);
    // }

    return (TemperatureData){temperature, timestamp};
}

// Updates the shared latest temperature state (intended to be called by the measurement task)
void internal_temp_sensor_update_latest_data(TemperatureData new_data) {
    if (xSemaphoreTake(s_int_temp_mutex, portMAX_DELAY) == pdTRUE) {
        s_current_int_temp_state = new_data;
        xSemaphoreGive(s_int_temp_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take mutex in internal_temp_sensor_update_latest_data");
    }
}

// Gets the latest temperature data safely (can be called by any task)
TemperatureData internal_temp_sensor_get_latest_data(void) {
    TemperatureData data;
    if (xSemaphoreTake(s_int_temp_mutex, portMAX_DELAY) == pdTRUE) {
        data = s_current_int_temp_state; // Copy the data
        xSemaphoreGive(s_int_temp_mutex);
    } else {
        // Handle error - return default/zero data?
        data = (TemperatureData){.temperature = -273.15f, .timestamp = 0}; // Indicate error/unavailable
        ESP_LOGE(TAG, "Failed to get internal temperature mutex in temp_meas_get_latest_data");
    }
    return data;
}

// Gets buffered temperature data since last call and resets the read pointer
int internal_temp_sensor_get_buffered_data(TemperatureData *out_buffer, int max_count) {
    int count = 0;
    int current_write_idx;
    bool was_buffer_full;

    // --- Critical section to read volatile variables consistently ---
    // Using a mutex here is safer if other tasks might access these indices,
    // but if only this function reads them, disabling interrupts might suffice.
    // Let's use the existing mutex for simplicity and safety.
    if (xSemaphoreTake(s_int_temp_mutex, pdMS_TO_TICKS(50)) != pdTRUE) { // Use timeout
        ESP_LOGE(TAG, "Failed to get mutex in temp_meas_get_buffered_data");
        return 0; // Indicate no data retrieved
    }

    current_write_idx = buffer_write_idx; // Capture current write index
    was_buffer_full = buffer_full;        // Capture full status

    int temp_read_idx = buffer_read_idx; // Local copy for iteration

    while (count < max_count) {
        if (!was_buffer_full && temp_read_idx == current_write_idx) {
            break; // Reached the write pointer in a non-full buffer
        }
        if (was_buffer_full && count >= DATA_BUFFER_SIZE) {
             break; // Copied the entire full buffer
        }

        out_buffer[count++] = temp_buffer[temp_read_idx];
        temp_read_idx = (temp_read_idx + 1) % DATA_BUFFER_SIZE;

        if (was_buffer_full && temp_read_idx == current_write_idx) break; // Copied up to write pointer in a full buffer
    }

    buffer_read_idx = temp_read_idx; // Update the read pointer
    buffer_full = false;             // Assume buffer is no longer full after reading
    xSemaphoreGive(s_int_temp_mutex);
    // --- End Critical Section ---

    return count;
}

// Gets fill percentage of buffer
int internal_temp_sensor_get_buffer_full_percentage() {
    int percentage = -1;
    if (xSemaphoreTake(s_int_temp_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        int count;
        if (buffer_full) {
            count = DATA_BUFFER_SIZE;
        } else {
            count = (buffer_write_idx - buffer_read_idx + DATA_BUFFER_SIZE) % DATA_BUFFER_SIZE;
        }
        percentage = (count * 100) / DATA_BUFFER_SIZE;
        xSemaphoreGive(s_int_temp_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take mutex in internal_temp_sensor_get_buffer_full_percentage");
        return -1; // Indicate error
    }
    return percentage;
}

// Gets the contents of the data buffer in json format
char* internal_temp_sensor_get_data_buffer_json() {
    int data_count = internal_temp_sensor_get_buffered_data(temp_buffer, DATA_BUFFER_SIZE);

    // Estimate required JSON buffer size:
    // Approx 50 chars per entry {"p":123.45,"t":12345.678}, + overhead
    size_t json_buffer_size = (data_count * 50) + 50; // +50 for base structure and safety
    char *json_buffer = malloc(json_buffer_size);
    if (!json_buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer for JSON response (%d bytes)", json_buffer_size);
        return NULL;
    }

    // Build the JSON string
    char *ptr = json_buffer;
    size_t remaining_len = json_buffer_size;
    int written;

    // Start JSON object and data array
    written = snprintf(ptr, remaining_len, "{\"status\":\"ok\",\"count\":%d,\"data\":[", data_count);
    ptr += written;
    remaining_len -= written;

    // Add each data point
    for (int i = 0; i < data_count && remaining_len > 1; i++) {
        written = snprintf(ptr, remaining_len, "%s{\"itemp\":%.2f,\"t\":%llu}",
                         (i > 0 ? "," : ""), // Add comma separator
                         temp_buffer[i].temperature,
                         temp_buffer[i].timestamp);
        if (written >= remaining_len) {
            ESP_LOGW(TAG, "JSON buffer potentially truncated");
            // Consider sending partial data or an error
            break;
        }
        ptr += written;
        remaining_len -= written;
    }

    // Close array and object
    snprintf(ptr, remaining_len, "]}");

    return json_buffer;
}