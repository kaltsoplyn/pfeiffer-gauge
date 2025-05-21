#pragma once

#include "esp_err.h"
#include "driver/usb_serial_jtag.h" // Needed for the full USB JTAG driver
#include "driver/usb_serial_jtag_vfs.h"

#define SERIAL_BUFFER_SIZE 256
#define SERIAL_STACK_SIZE 2048

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the serial component (if needed)
esp_err_t serial_comp_init(void);

// // Sends a null-terminated string over serial
// void serial_comp_send_string(const char* str);

// // Reads a line of text from serial, blocks until a line is received or buffer is full
// // Returns the number of characters read (the length of the incoming buffer, excluding null terminator), or -1 on error.
// int serial_comp_read_line(char* buffer, int max_len);


/**
 * @brief Reads data from the serial component into the provided buffer.
 *
 * This function attempts to read data from the serial interface and stores it
 * into the buffer provided by the caller.
 *
 * @param buffer Pointer to a character array where the read data will be stored.
 *               The buffer must be allocated by the caller and should be large
 *               enough to hold the expected data.
 *
 * @return The number of bytes read on success, or a negative value on error.
 */
int serial_comp_receive(char *buffer);

/**
 * @brief Sends a string over the serial interface.
 *
 * This function transmits the specified null-terminated string via the serial communication
 * interface implemented by the serial_comp component.
 *
 * @param str Pointer to the null-terminated string to be sent.
 */
void serial_comp_send(const char* str);

#ifdef __cplusplus
}
#endif