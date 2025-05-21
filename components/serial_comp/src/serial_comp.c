#include "serial_comp.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#define DATA_CHUNK_SIZE 24
static uint8_t *data;

static const char *TAG = "serial_comp";

esp_err_t serial_comp_init(void) {
    ESP_LOGI(TAG, "Initializing USB Serial/JTAG for standard blocking I/O...");

    // Configuration for the USB Serial/JTAG driver
    // Default buffer sizes are usually sufficient.
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .tx_buffer_size = SERIAL_BUFFER_SIZE, // TX buffer size
        .rx_buffer_size = SERIAL_BUFFER_SIZE, // RX buffer size
    };

    // 1. Install the USB Serial/JTAG driver
    esp_err_t err = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB Serial/JTAG driver: %s", esp_err_to_name(err));
        return err;
    }

    // usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CRLF);
    // usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    // // 2. Tell VFS to use the driver for stdin/stdout/stderr
    // // This enables standard blocking behavior for reads.
    // usb_serial_jtag_vfs_use_driver();


    ESP_LOGI(TAG, "USB Serial/JTAG driver installed and VFS configured for blocking reads.");
    return ESP_OK;
}

void serial_comp_send(const char* str) {
    int len = strlen(str);

    if (str == NULL || len == 0) {
        ESP_LOGE(TAG, "Cannot send NULL or empty string");
        return;
    }

    for (int i = 0; i < len; i++) {
        usb_serial_jtag_write_bytes((uint8_t *)&str[i], 1, 20 / portTICK_PERIOD_MS);
    }
    char newline = '\n';
    usb_serial_jtag_write_bytes((uint8_t *)&newline, 1, 20 / portTICK_PERIOD_MS);

    // VFS implementation below:
    //printf("%s\n", str);
    // You might want to explicitly flush stdout if you're not sending newlines regularly
    // or if you experience buffering issues. Good practice for prompt sending.
    //fflush(stdout);
    ESP_LOGD(TAG, "Sent: %s", str);
}

// int serial_comp_read_line(char* buffer, int max_len) {
//     if (buffer == NULL || max_len <= 0) {
//         ESP_LOGE(TAG, "Invalid buffer or max_len for read_line");
//         return -1;
//     }

//     if (fgets(buffer, max_len, stdin)) {
//         // Remove newline characters (CRLF or LF)
//         buffer[strcspn(buffer, "\r\n")] = '\0';
//         ESP_LOGD(TAG, "Read: %s", buffer);
//         return strlen(buffer);
//     }
//     if (strlen(buffer) == 0) {
//         ESP_LOGW(TAG, "Received 0-length buffer (NULL or EOF) >>%s<<", buffer);
//         return -1;
//     }
//     ESP_LOGI(TAG, "Received buffer >>%s<<", buffer);
//     return 0; // Error or EOF
// }


int serial_comp_receive(char *buffer) {
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Invalid or uninitialized buffer for read");
        return -1;
    }

    data = (uint8_t *) malloc(DATA_CHUNK_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Couldn't allocate memory to capture incoming characters");
        if (data) free(data);
        return -1;
    }

    int buf_len = 0;
    buffer[0] = '\0';

    while (1) {
        // Read a chunk of bytes into the 'data' buffer.
        // The second argument to usb_serial_jtag_read_bytes is the size of the buffer
        int bytes_read = usb_serial_jtag_read_bytes(data, DATA_CHUNK_SIZE, 20 / portTICK_PERIOD_MS);

        if (bytes_read > 0) {
            for (int i = 0; i < bytes_read; i++) {
                char current_char = (char)data[i];
                usb_serial_jtag_write_bytes(data, bytes_read, 20 / portTICK_PERIOD_MS);

                if (current_char == '\n' || current_char == '\r') {
                    // Some systems send CR on enter, some send LF or CRLF.
                    // (my windows send CR)

                    buffer[buf_len] = '\0'; // Null-terminate the string (excluding the newline)
                    
                    ESP_LOGI(TAG, "Received command: %s", buffer);

                    return buf_len;
                    
                    // Clear 'out' buffer for the next line
                    // buf_len = 0;
                    // buffer[0] = '\0';
                } else {
                    // keep the -1 for the null terminator
                    if (buf_len < SERIAL_BUFFER_SIZE - 1) {
                        buffer[buf_len] = current_char;
                        buf_len++;
                        
                        buffer[buf_len] = '\0'; // null-terminate always > good practice
                    } else {
                        // Buffer full
                        ESP_LOGE(TAG, "Line buffer overflow. Discarding current line fragment.");
                        // Discard and reset
                        buf_len = 0;
                        buffer[0] = '\0';
                        return -1;
                    }
                }
            }
        }
    }


}
