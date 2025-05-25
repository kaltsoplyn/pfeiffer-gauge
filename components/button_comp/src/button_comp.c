#include "button_comp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "app_manager.h"
#include "esp_log.h"


static TimerHandle_t button_timer;
static bool button_pressed = false;

static const char *TAG = "ButtonComp";

static void button_isr_handler(void *arg) {
    xTimerStart(button_timer, 0);
}

static void button_timer_callback(TimerHandle_t xTimer) {
    button_pressed = true;
    ESP_LOGI(TAG, "Button long press detected - invoking callback");
    bool serial_stream_active = app_manager_get_serial_data_json_stream_active();
    if (serial_stream_active) {
        ESP_LOGI(TAG, "Serial stream is active. Stopping it.");
        app_manager_set_serial_data_json_stream_active(false);
    } else {
        ESP_LOGI(TAG, "Serial stream is NOT active. Starting it.");
        app_manager_set_serial_data_json_stream_active(true);
    }
}

esp_err_t button_comp_init() {
    // Initialize button for reset
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init GPIO!\n%s", esp_err_to_name(ret));
        return ret;
    }
    
    button_timer = xTimerCreate("button_timer", pdMS_TO_TICKS(CONFIG_BUTTON_PRESS_MS), pdFALSE, NULL, button_timer_callback);
    if (button_timer == NULL) ret = ESP_FAIL;

    ret = ret == ESP_OK ? gpio_install_isr_service(0) : ret;
    ret = ret == ESP_OK ? gpio_isr_handler_add(CONFIG_BUTTON_GPIO, button_isr_handler, NULL) : ret;

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init GPIO!\n%s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Button component initialized.");

    return ESP_OK;
}