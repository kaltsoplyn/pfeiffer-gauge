/*
 * Component for reading pressure from Pfeiffer gauge via ADC
*/

#include "pressure_meas_comp.h"
#include "freertos/FreeRTOS.h" // Required for mutex
#include "freertos/semphr.h"  // Required for mutex
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"
#include "time.h"
#include "esp_log.h"

#define VOLTAGE_DIVIDER_RATIO           4.4375   // gauge is up to 11V (F.S. is at 9V), (55+16)/16 = 4.4375 divider brings down within the 3.3V range (resistor values in kOhms)
#define ADC_CHANNEL                     ADC_CHANNEL_2
#define ADC_BITWIDTH                    ADC_BITWIDTH_12
#define ADC_ATTENUATION                 ADC_ATTEN_DB_12  // Supposedely ADC_ATTEN_DB_12 → 150 mV ~ 2450 mV
#define ADC_UNIT_ID                     ADC_UNIT_1
#define MOCK                            false // Set to true for mock data

static const char *TAG = "PressureMeas";

// --- Circular Buffer ---
static PressureData pressure_buffer[DATA_BUFFER_SIZE];
static volatile int buffer_write_idx = 0; // Index where next measurement will be written
static volatile int buffer_read_idx = 0;  // Index where the API last read up to
static volatile bool buffer_full = false; // Flag to indicate if buffer has wrapped around


// --- State Variables ---
static SemaphoreHandle_t s_pressure_mutex = NULL;
static PressureData s_current_pressure_state = {.pressure = -1.0f, .timestamp = 0}; // Initialize to invalid
static adc_oneshot_unit_handle_t adc1_handle; // Keep handle accessible

// --- Mock Data ---
static int mock_previous_adc;
#if MOCK
mock_previous_adc = 2000;
#endif

esp_err_t pressure_meas_init() {
    if (MOCK) {
        ESP_LOGI(TAG, "Initializing pressure measurement in MOCK mode.");
        srand(time(NULL)); // MOCK
        // Initialize mutex even in mock mode if getter/setter functions might be called
        s_pressure_mutex = xSemaphoreCreateMutex();
        return (s_pressure_mutex == NULL) ? ESP_FAIL : ESP_OK; // Return status based on mutex creation
    }

    // adc_oneshot_unit_init_cfg_t adc_config = {
    //     .unit_id = ADC_UNIT_ID
    // };

    // esp_err_t ret = adc_oneshot_new_unit(&adc_config, &adc1_handle);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
    //     return ret;
    // }
    // ESP_LOGI(TAG, "ADC Unit Initialized.");

    adc1_handle = sensor_types_get_adc_unit_handle();

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTENUATION
    };
    esp_err_t ret = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        // Consider cleaning up the ADC unit handle here if necessary
        adc_oneshot_del_unit(adc1_handle); // Clean up allocated unit
        return ret;
    }
    ESP_LOGI(TAG, "ADC Channel Configured.");

    // Initialize mutex
    s_pressure_mutex = xSemaphoreCreateMutex();
    if (s_pressure_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create pressure mutex!");
        adc_oneshot_del_unit(adc1_handle); // Clean up allocated unit
        return ESP_FAIL; // Or ESP_ERR_NO_MEM
    }

    ESP_LOGI(TAG, "Pressure measurement initialized and enabled.");
    return ESP_OK; // Success
}

static int read_adc_value() {
    if (MOCK) {
        int rand_adc = rand() % 101 - 50; // keep in range -50 to 50
        mock_previous_adc = mock_previous_adc + rand_adc;
        if (mock_previous_adc > 4095) mock_previous_adc = 4095;
        if (mock_previous_adc < 0) mock_previous_adc = 0;
        return mock_previous_adc;
    }

    int raw_value = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw_value));
    return raw_value;
}

float convert_to_pressure(int adc_value) {
    //printf(" -- raw value: %d\n", adc_value);
    float voltage = (adc_value / 4095.0f) * 3.3 * VOLTAGE_DIVIDER_RATIO;  // Assuming 12-bit resolution
    //printf(" ---- voltage: %.2f\n", voltage);
    float pressure_mbar = (voltage - 1) * 0.125 * PRESSURE_GAUGE_FS;  // P(mbar) = (V - 1) * 0.125 * F.S.
    return pressure_mbar;
}

// Reads ADC, converts to pressure, and returns the raw data point
PressureData pressure_meas_read_raw() {
    int adc_value = read_adc_value();
    float pressure = convert_to_pressure(adc_value);
    uint64_t timestamp = time_manager_get_timestamp_ms();  //esp_timer_get_time() / 1000;

    PressureData current_measurement = {pressure, timestamp};

    // --- Store in circular buffer - Use mutex for shared state variables ---
    if (xSemaphoreTake(s_pressure_mutex, pdMS_TO_TICKS(10)) == pdTRUE) { // Use short timeout
    pressure_buffer[buffer_write_idx] = current_measurement;
    buffer_write_idx = (buffer_write_idx + 1) % DATA_BUFFER_SIZE;

    if (buffer_full && buffer_write_idx == buffer_read_idx) {
        // Overwriting data that hasn't been read yet, advance read pointer
        buffer_read_idx = (buffer_read_idx + 1) % DATA_BUFFER_SIZE;
        // buffer_full remains true
    } else if (buffer_write_idx == buffer_read_idx) {
        buffer_full = true; // Buffer just became full or wrapped exactly
    }

        xSemaphoreGive(s_pressure_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take mutex in pressure_meas_read_raw - data point lost");
        // Handle the case where data couldn't be written (e.g., skip this point)
    }
    // --- End Store ---

    if (/** pressure < 0 || */ pressure > PRESSURE_GAUGE_FS) { // TODO uncomment in real-life app
        ESP_LOGW(TAG, "Pressure outside acceptable range: %.2f mbar", pressure);
    }

    return (PressureData){pressure, timestamp};
}

// Updates the shared latest pressure state (intended to be called by the measurement task)
void pressure_meas_update_latest_data(PressureData new_data) {
    if (xSemaphoreTake(s_pressure_mutex, portMAX_DELAY) == pdTRUE) {
        s_current_pressure_state = new_data;
        xSemaphoreGive(s_pressure_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take mutex in pressure_meas_update_latest_data");
    }
}

// Gets the latest pressure data safely (can be called by any task)
PressureData pressure_meas_get_latest_data(void) {
    PressureData data;
    if (xSemaphoreTake(s_pressure_mutex, portMAX_DELAY) == pdTRUE) {
        data = s_current_pressure_state; // Copy the data
        xSemaphoreGive(s_pressure_mutex);
    } else {
        // Handle error - return default/zero data?
        data = (PressureData){.pressure = -1.0f, .timestamp = 0}; // Indicate error/unavailable
        ESP_LOGE(TAG, "Failed to get pressure mutex in pressure_meas_get_latest_data");
    }
    return data;
}

// Gets buffered pressure data since last call and resets the read pointer
int pressure_meas_get_buffered_data(PressureData *out_buffer, int max_count) {
    int count = 0;
    int current_write_idx;
    bool was_buffer_full;

    // --- Critical section to read volatile variables consistently ---
    // Using a mutex here is safer if other tasks might access these indices,
    // but if only this function reads them, disabling interrupts might suffice.
    // Let's use the existing mutex for simplicity and safety.
    if (xSemaphoreTake(s_pressure_mutex, pdMS_TO_TICKS(50)) != pdTRUE) { // Use timeout
        ESP_LOGE(TAG, "Failed to get mutex in pressure_meas_get_buffered_data");
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

        out_buffer[count++] = pressure_buffer[temp_read_idx];
        temp_read_idx = (temp_read_idx + 1) % DATA_BUFFER_SIZE;

        if (was_buffer_full && temp_read_idx == current_write_idx) break; // Copied up to write pointer in a full buffer
    }

    buffer_read_idx = temp_read_idx; // Update the read pointer
    buffer_full = false;             // Assume buffer is no longer full after reading
    xSemaphoreGive(s_pressure_mutex);
    // --- End Critical Section ---

    return count;
}

int pressure_meas_get_buffer_full_percentage() {
    int percentage = -1;
    if (xSemaphoreTake(s_pressure_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        int count;
        if (buffer_full) {
            count = DATA_BUFFER_SIZE;
        } else {
            count = (buffer_write_idx - buffer_read_idx + DATA_BUFFER_SIZE) % DATA_BUFFER_SIZE;
        }
        percentage = (count * 100) / DATA_BUFFER_SIZE;
        xSemaphoreGive(s_pressure_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take mutex in pressure_meas_get_buffer_full_percentage");
        return -1; // Indicate error
    }
    return percentage;
}


char* pressure_meas_get_data_buffer_json() {
    int data_count = pressure_meas_get_buffered_data(pressure_buffer, DATA_BUFFER_SIZE);

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
        written = snprintf(ptr, remaining_len, "%s{\"pres\":%.2f,\"t\":%llu}",
                         (i > 0 ? "," : ""), // Add comma separator
                         pressure_buffer[i].pressure,
                         pressure_buffer[i].timestamp);
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