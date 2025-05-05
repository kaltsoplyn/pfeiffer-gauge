#include "uart_handler.h"
// #include "driver/uart.h"

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// #define UART_PORT_NUM      UART_NUM_0
// #define UART_BAUD_RATE     115200
// #define UART_RX_BUF_SIZE   1024
// #define TASK_STACK_SIZE    2048

// static bool space_pressed_flag = false;

// static void uart_event_task(void *pvParameters) {
//     uint8_t data[UART_RX_BUF_SIZE];
    
//     while (1) {
//         int len = uart_read_bytes(UART_PORT_NUM, data, UART_RX_BUF_SIZE, 20 / portTICK_PERIOD_MS);
//         for (int i = 0; i < len; i++) {
//             if (data[i] == 0x20) {
//                 space_pressed_flag = !space_pressed_flag;
//             }
//         }
//     }
// }

// void uart_monitor_init(void) {
//     uart_config_t uart_config = {
//         .baud_rate = UART_BAUD_RATE,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_DEFAULT,
//     };
    
//     ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_RX_BUF_SIZE, 0, 0, NULL, 0));
//     ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
//     ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
//     xTaskCreate(uart_event_task, "uart_event_task", TASK_STACK_SIZE, NULL, 10, NULL);
// }

// bool get_space_pressed_flag(void) {
//     return space_pressed_flag;
// }

// void set_space_pressed_flag(bool state) {
//     space_pressed_flag = state;
// }

void uart_monitor_init() {

}

bool get_space_pressed_flag() {
    return false;
}

void set_space_pressed_flag(bool state) {
    
}