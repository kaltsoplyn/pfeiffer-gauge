/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
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
 #include "lvgl_display.h"
 #include "esp_timer.h"
 #include "network_comp.h"
 #include "wifi_manager.h"
 #include "internal_temp_sensor.h"

 #define DEFAULT_SAMPLING_INTERVAL_MS            100

 #define DEBUG_RUN_TASKS                         true // Enable/disable tasks

static const char *TAG = "AppMain";
static float t0;

typedef struct {
    int sampling_interval_ms;
    bool is_sampling_active;
    float internal_temp;
} State;


static State state;

void set_sampling_interval(int interval_ms) {
    state.sampling_interval_ms = interval_ms;
}

void reset_sampling_interval() {
    state.sampling_interval_ms = DEFAULT_SAMPLING_INTERVAL_MS;
}

void pressure_measurement_task(void *arg) {
    while (1) {
        if (state.is_sampling_active) {
            // Read the raw pressure data
            PressureData new_data = pressure_meas_read_raw();
            // Update the centrally stored, thread-safe state
            pressure_meas_update_latest_data(new_data);
        }
        vTaskDelay(pdMS_TO_TICKS(state.sampling_interval_ms));
    }
}

void update_display_pressure_task(void *arg) {
    float pressure;
    bool active;
    int buffer_full_percentage;

    while (1) {
        active = state.is_sampling_active; // Check if sampling is active
        PressureData current_data = pressure_meas_get_latest_data(); // Get latest data safely
        pressure = current_data.pressure;
        buffer_full_percentage = pressure_meas_get_buffer_full_percentage();
        if (active) {
            lvgl_display_pressure(pressure, PRESSURE_GAUGE_FS);
            lvgl_display_buffer_pc(buffer_full_percentage);
        } else {
            lvgl_display_pressure(-1.0f, PRESSURE_GAUGE_FS);
            lvgl_display_buffer_pc(-1);
        }
        vTaskDelay(pdMS_TO_TICKS(2 * state.sampling_interval_ms));
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

void update_display_internal_temp(void *arg) {
    while (1) {
        char temp_text[20];
        float temp = 0;
        float* temp_ptr = &temp;
        esp_err_t ret = internal_temp_sensor_read(temp_ptr);
        if (ret == ESP_OK) {
            snprintf(temp_text, sizeof(temp_text), "T: %.1f°C", temp);
            lvgl_display_internal_temp(temp_text);
            state.internal_temp = temp;
        } else {
            lvgl_display_internal_temp("T: [ ERR ]");
            ESP_LOGE(TAG, "Failed to read internal temperature: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void app_main(void)
{
    t0 = esp_timer_get_time() / 1000.0f; // start time of the app

    // Initialize components
    network_comp_init();
    
    esp_err_t pressure_init_err = pressure_meas_init(); // Initialize pressure component (ADC, mutex, state)
    if (pressure_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize pressure measurement component!\n%s", esp_err_to_name(pressure_init_err));  
    }

    esp_err_t temp_init_err = internal_temp_sensor_init();
    if (temp_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize internal temperature sensor!\n%s", esp_err_to_name(temp_init_err));  
    }

    lvgl_display_init();

    printf("Pfeiffer CMR362");

    state = (State){
        .sampling_interval_ms = DEFAULT_SAMPLING_INTERVAL_MS,
        .is_sampling_active = true
    };

    

    //if (MOCK) mock_previous_adc = 2000;

    if (DEBUG_RUN_TASKS) {

        xTaskCreate(pressure_measurement_task, "pressure_measurement_task", 2048, NULL, 5, NULL);
        xTaskCreate(update_display_pressure_task, "update_display_pressure_task", 2048, NULL, 2, NULL);
        xTaskCreate(update_display_ipaddr_task, "update_display_ipaddr_task", 2048, NULL, 2, NULL);
        xTaskCreate(update_display_internal_temp, "update_display_internal_temp", 2048, NULL, 2, NULL);

    }
    

    //xTaskCreate(uart_task, "uart_task", 2048, NULL, 2, NULL);


}
