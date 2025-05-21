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
 #include "time_manager.h"
 #include "pressure_meas_comp.h"
 #include "temp_meas_comp.h"
 #include "app_manager.h"
 #include "lvgl_display.h"
 #include "esp_timer.h"
 #include "network_comp.h"
 #include "wifi_manager.h"
 #include "internal_temp_sensor.h"
 #include "serial_comp.h"


// basic flow: sensor_types.h -> pressure, temp, internal temp, and app_manager components -> network component and this
 

 #define DEBUG_RUN_TASKS                         true // Enable/disable tasks
 #define SERIAL_COMMAND_BUFFER_SIZE 256 // Adjust as needed
static char s_serial_buffer[SERIAL_COMMAND_BUFFER_SIZE];

static const char *TAG = "AppMain";
static float t0;

// --- Tasks ---

// TODO: combine sensor measurements into a sensors component
void log_test_task(void *arg) {
    while (1) {
        ESP_LOGI(TAG, "Logging test task running...");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

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

            //PressureData new_pressure_data = (PressureData){-1.0f, 0};         //MOCK
            //TemperatureData new_temp_data = (TemperatureData){-1.0f, 0};       //MOCK
            //TemperatureData new_int_temp_data = (TemperatureData){-1.0f, 0};   //MOCK
            SensorData_t new_sensor_data = (SensorData_t){new_pressure_data, new_temp_data, new_int_temp_data};
            app_manager_update_latest_sensor_data(new_sensor_data);

            if (app_manager_get_serial_data_json_stream()) {
                // Get sensor data as a JSON string from app_manager
                ESP_LOGI(TAG, "Testing JSON streaming clause");
                char *json_string = app_manager_get_latest_sensor_data_json();
                if (json_string) {
                    serial_comp_send(json_string);
                    free(json_string);
                } else {
                    ESP_LOGE(TAG, "Failed to get JSON string for serial streaming.");
                }
            }
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
    char ipaddr_colon_port[60];
    char ipaddr[40]; // ipv4 or ipv6
    if (wifi_get_ip_address(ipaddr)) {
        snprintf(ipaddr_colon_port, sizeof(ipaddr_colon_port), "IP: %s:%d", ipaddr, CONFIG_WEB_PORT);
        lvgl_display_ipaddr(ipaddr_colon_port);
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


// void serial_command_task(void *arg) {
//     ESP_LOGI(TAG, "Serial command task started. Waiting for commands...");

//     vTaskDelay(pdMS_TO_TICKS(500));

//     while (1) {
//         // serial_comp_read_line will block until a line is received or an error occurs
//         int len = serial_comp_read_line(s_serial_buffer, SERIAL_COMMAND_BUFFER_SIZE);

//         if (len > 0) {
//             ESP_LOGI(TAG, "Received command: '%s'", s_serial_buffer);

//             // --- Command Processing Logic ---
//             // Example: Simple command parsing
//             if (strcmp(s_serial_buffer, "GET_STATUS") == 0) {
//                 ESP_LOGI(TAG, "Processing GET_STATUS command...");
//                 // Example: Send back some status information
//                 SensorData_t current_data = app_manager_get_latest_sensor_data();
//                 char status_response[100];
//                 snprintf(status_response, sizeof(status_response),
//                          "Status: P=%.2f, T=%.2f, IT=%.2f",
//                          current_data.pressure_data.pressure,
//                          current_data.temperature_data.temperature,
//                          current_data.internal_temp_data.temperature);
//                 serial_comp_send_string(status_response);

//             } else if (strcmp(s_serial_buffer, "TOGGLE_SAMPLING") == 0) {
//                 bool current_sampling_state = app_manager_get_sampling_active();
//                 app_manager_set_sampling_active(!current_sampling_state);
//                 ESP_LOGI(TAG, "Sampling toggled to: %s", !current_sampling_state ? "ON" : "OFF");
//                 serial_comp_send_string(!current_sampling_state ? "Sampling ON" : "Sampling OFF");

//             } else if (strcmp(s_serial_buffer, "TOGGLE_WEB_SERVER") == 0) {
//                 esp_err_t ret = network_comp_toggle_web_server();
//                 if (ret == ESP_OK) {
//                     bool server_on = app_manager_get_web_server_active();
//                     serial_comp_send_string(server_on ? "Web server ON" : "Web server OFF");
//                 } else {
//                     ESP_LOGE(TAG, "Failed to toggle web server: %s", esp_err_to_name(ret));
//                 }

//             } else if (strncmp(s_serial_buffer, "ECHO ", strlen("ECHO ")) == 0) {
//                 serial_comp_send_string(s_serial_buffer + strlen("ECHO "));

//             } else if (strncmp(s_serial_buffer, "SET_INTERVAL ", strlen("SET_INTERVAL ")) == 0) {
//                 int interval_ms = atoi(s_serial_buffer + strlen("SET_INTERVAL "));
//                 if (interval_ms >= 5) { // Min interval check from app_manager
//                     if (app_manager_set_sampling_interval_ms(interval_ms) == ESP_OK) {
//                         char response[64];
//                         snprintf(response, sizeof(response), "Sampling interval set to %d ms", interval_ms);
//                         serial_comp_send_string(response);
//                     } else {
//                         serial_comp_send_string("Error setting interval (invalid or mutex issue).");
//                     }
//                 } else {
//                     serial_comp_send_string("Invalid interval. Min 5 ms.");
//                 }
//             } else {
//                 ESP_LOGW(TAG, "Unknown command: '%s'", s_serial_buffer);
//                 serial_comp_send_string("Unknown command");
//             }
//             // --- End Command Processing Logic ---

//         } else if (len == -1) {
//             // Error or EOF from serial_comp_read_line
//             // ESP_LOGW(TAG, "Error reading from serial or EOF.");
//             // You might want a small delay here if errors are frequent,
//             // but fgets on stdin usually blocks, so this path might be rare
//             // unless the USB connection is unstable.
//             vTaskDelay(pdMS_TO_TICKS(100)); // Small delay on error
//         }
//         // No explicit vTaskDelay needed in the main success path because
//         // serial_comp_read_line (using fgets) is blocking.
//     }
// }

void serial_comp_echo_task(void *arg) {
    ESP_LOGI(TAG, "Serial command task started. Waiting for commands...");

    while(1) {
        
        int len = serial_comp_receive(s_serial_buffer);
        if (len > 0) {
            ESP_LOGI(TAG, "Read %d bytes: %s", len, s_serial_buffer);
        
            // Handle commands
            if (strncmp(s_serial_buffer, "data", 4) == 0) {
                serial_comp_send(app_manager_get_latest_sensor_data_json());
            } else if (strncmp(s_serial_buffer, "backlight", 9) == 0) {
                if (strcmp(s_serial_buffer + 10, "on") == 0) {
                    lvgl_display_backlight(true);
                } else if (strcmp(s_serial_buffer + 10, "off") == 0) {
                    lvgl_display_backlight(false);
                }
            } else if (strncmp(s_serial_buffer, "webserver", 9) == 0) {
                    network_comp_toggle_web_server();
            } else {
                ESP_LOGW(TAG, "Unknown command: '%s'", s_serial_buffer);
            }

        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}


void app_main(void) {
    // Initialize logging
    esp_log_level_set("*", ESP_LOG_INFO);  // global log level
    ESP_LOGI(TAG, "Starting %s controller...", PRESSURE_GAUGE_NAME);

    // Add delay to ensure UART is ready
    vTaskDelay(pdMS_TO_TICKS(100));


    // Initialize components
    esp_err_t sensor_types_init_err = adc_init(); // this has to go first
    if (sensor_types_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sensor types and adc unit!\n%s", esp_err_to_name(sensor_types_init_err));
    }

    esp_err_t app_manager_init_err = app_manager_init();
    if (app_manager_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize application manager!\n%s", esp_err_to_name(app_manager_init_err));
    };
    
    esp_err_t network_init_err = network_comp_init();
    if (network_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network component.\n%s", esp_err_to_name(network_init_err));
    } else {
        // Initialize Time Manager after network component (which should handle Wi-Fi connection)
        // SNTP will start trying to sync once Wi-Fi is connected.
        esp_err_t time_init_err = time_manager_init();
        if (time_init_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize Time Manager: %s", esp_err_to_name(time_init_err));
        }
    }

    esp_err_t serial_comp_init_err = serial_comp_init();
    if (serial_comp_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize serial component!\n%s", esp_err_to_name(serial_comp_init_err));
    };

    time_t t0 = time_manager_get_timestamp_ms();


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

    ESP_LOGI(TAG, "%s - Initialization complete", PRESSURE_GAUGE_NAME);

    if (DEBUG_RUN_TASKS) {

        xTaskCreate(sensor_measurement_task,      "sensor_measurement_task",      2048, NULL, 5, NULL);
        xTaskCreate(update_sensor_display_task,     "update_sensor_display_task",   2048, NULL, 2, NULL);
        xTaskCreate(update_display_ipaddr_task,     "update_display_ipaddr_task",   2048, NULL, 2, NULL);
        //xTaskCreate(serial_command_task,          "serial_command_task",          2560, NULL, 3, NULL);
        xTaskCreate(serial_comp_echo_task,          "serial_echo_task",          2560, NULL, 3, NULL);

        //xTaskCreate(log_test_task,          "log_test_task",          2018, NULL, 1, NULL);

    }

    printf("Started -- this message is to test printf over JTAG...\n");

}
