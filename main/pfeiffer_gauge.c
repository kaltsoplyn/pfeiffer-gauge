/*
 * 
 * Pfeiffer pressure gauge + 10kOhm thermistor measurements
 * Display pressure, temperature, and SoC temperature on integrated display
 * By default, web server is running to serve the data, on DHCP address (or 192.168.4.1), port 80
 * 
 * Routes:
 *   /          -> connect to WiFi
 *   /data      -> display the SensorData json
 *   /api/data  -> pure SensorData json body
 * 
 * The button connected to GPIO 5 does pretty much nothing for now :)
 *
 * This was designed for an ESP32-C6 board with an integrated ST7789 display
 * 
 * Licence: none - use this freely
 * 
 * © 2025 - Yio Cat (kaltsoplyn)
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/semphr.h"
 #include "soc/soc_caps.h"
 #include "esp_log.h"
 #include "esp_adc/adc_oneshot.h"
 #include "esp_adc/adc_cali.h"
 #include "esp_adc/adc_cali_scheme.h"
 #include "time.h"
 #include "pressure_meas_comp.h"
 #include "temp_meas_comp.h"
 #include "app_manager.h"
 #include "lvgl_display.h"
 #include "esp_timer.h"
 #include "network_comp.h"
 #include "wifi_manager.h"
 #include "internal_temp_sensor.h"

// basic flow: sensor_types.h -> pressure, temp, internal temp, and app_manager components -> network component and this
 

 #define DEBUG_RUN_TASKS                         true // Enable/disable tasks

static const char *TAG = "AppMain";
static float t0;

// --- Tasks ---

// TODO: combine sensor measurements into a sensors component
void sensor_measurement_task(void *arg) {
    while (1) {
        bool is_sampling_active = app_manager_get_sampling_active();
        int sampling_interval_ms = app_manager_get_sampling_interval_ms();
        if (is_sampling_active) {
            // Read the raw pressure data
            PressureData new_pressure_data = pressure_meas_read_raw();
            pressure_meas_update_latest_data(new_pressure_data);
            // Read the raw temperature data
            TemperatureData new_temp_data = temp_meas_read_raw();
            temp_meas_update_latest_data(new_temp_data);
            // Read the raw internal temperature data
            TemperatureData new_int_temp_data = internal_temp_sensor_read_raw();
            internal_temp_sensor_update_latest_data(new_int_temp_data);
            // Update state
            SensorData_t new_sensor_data = (SensorData_t){new_pressure_data, new_temp_data, new_int_temp_data};
            app_manager_update_latest_sensor_data(new_sensor_data);
        }
        vTaskDelay(pdMS_TO_TICKS(sampling_interval_ms));
    }
}

void update_sensor_display_task(void *arg) {
    float pressure;
    float temp;
    float internal_temp;
    bool active;
    int pressure_buffer_full_percentage;
    int temp_buffer_full_percentage;
    int internal_temp_buffer_full_percentage;
    int display_update_interval_ms = app_manager_get_display_update_interval_ms();

    while (1) {
        active = app_manager_get_sampling_active(); // Check if sampling is active

        SensorData_t latest_sensor_data = app_manager_get_latest_sensor_data();
        pressure = latest_sensor_data.pressure_data.pressure;
        temp = latest_sensor_data.temperature_data.temperature;
        internal_temp = latest_sensor_data.internal_temp_data.temperature;

        pressure_buffer_full_percentage = pressure_meas_get_buffer_full_percentage();
        temp_buffer_full_percentage = temp_meas_get_buffer_full_percentage();
        internal_temp_buffer_full_percentage = internal_temp_sensor_get_buffer_full_percentage();
        if (pressure_buffer_full_percentage != temp_buffer_full_percentage || 
            pressure_buffer_full_percentage != internal_temp_buffer_full_percentage) {
            ESP_LOGW(TAG, "Pressure, temperature, and internal temperature buffers are not equally filled: %d vs %d vs %d %%", 
                pressure_buffer_full_percentage, 
                temp_buffer_full_percentage,
                internal_temp_buffer_full_percentage);
        }
        if (active) {
            lvgl_display_pressure(pressure, PRESSURE_GAUGE_FS);
            lvgl_display_buffer_pc(pressure_buffer_full_percentage);
            lvgl_display_temperature(temp);
            lvgl_display_internal_temp(internal_temp);
        } else {
            lvgl_display_pressure(-1.0f, PRESSURE_GAUGE_FS);
            lvgl_display_buffer_pc(-1);
            lvgl_display_temperature(-1000.0f);
            lvgl_display_internal_temp(-1000.0f);
        }
        vTaskDelay(pdMS_TO_TICKS(display_update_interval_ms));
    }
}

void update_display_ipaddr_handler() {
    char ipaddr_port[60];
    char ipaddr[40]; // ipv4 or ipv6
    if (wifi_get_ip_address(ipaddr)) {
        snprintf(ipaddr_port, sizeof(ipaddr_port), "IP: %s:%d", ipaddr, CONFIG_WEB_PORT);
        lvgl_display_ipaddr(ipaddr_port);
    } else {
        lvgl_display_ipaddr("IP: [ ERR ]");
    }
}

void update_display_ipaddr_task(void *arg) {
    while (1) {
        update_display_ipaddr_handler();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}


void app_main(void) {
    // Initialize logging first
    esp_log_level_set("*", ESP_LOG_INFO);  // Set global log level
    ESP_LOGI(TAG, "Starting %s controller...", PRESSURE_GAUGE_NAME);

    // Add delay to ensure UART is ready
    vTaskDelay(pdMS_TO_TICKS(100));


    // Initialize components
    esp_err_t sensor_types_init_err = adc_init();
    if (sensor_types_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sensor types and adc unit!\n%s", esp_err_to_name(sensor_types_init_err));
    }


    esp_err_t app_manager_init_err = app_manager_init();
    if (app_manager_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize application manager!\n%s", esp_err_to_name(app_manager_init_err));
    };

    uint64_t t0 = app_manager_get_start_time_ms();


    esp_err_t lvgl_init_err = lvgl_display_init();
    if (lvgl_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL display!\n%s", esp_err_to_name(lvgl_init_err));
    }

    esp_err_t pressure_init_err = pressure_meas_init(); // Initialize pressure component (ADC, mutex, state)
    if (pressure_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize pressure measurement component!\n%s", esp_err_to_name(pressure_init_err));  
    }

    esp_err_t temp_init_err = temp_meas_init();
    if (temp_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize temperature sensor!\n%s", esp_err_to_name(temp_init_err));  
    }

    esp_err_t internal_temp_init_err = internal_temp_sensor_init();
    if (internal_temp_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize internal temperature sensor!\n%s", esp_err_to_name(internal_temp_init_err));  
    }

    esp_err_t network_init_err = network_comp_init();
    if (network_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network component.\n%s", esp_err_to_name(network_init_err));
    }


    ESP_LOGI(TAG, "%s - Initialization complete", PRESSURE_GAUGE_NAME);

    //if (MOCK) mock_previous_adc = 2000;

    if (DEBUG_RUN_TASKS) {

        xTaskCreate(sensor_measurement_task, "sensor_measurement_task", 2048, NULL, 5, NULL);
        xTaskCreate(update_sensor_display_task, "update_sensor_display_task", 2048, NULL, 2, NULL);
        xTaskCreate(update_display_ipaddr_task, "update_display_ipaddr_task", 2048, NULL, 2, NULL);
    }
    

    //xTaskCreate(uart_task, "uart_task", 2048, NULL, 2, NULL);


}
