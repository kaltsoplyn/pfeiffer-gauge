#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PRESSURE_GAUGE_NAME "Pfeiffer CMR362"


/**
 * @brief Initialize the LVGL display.
 *
 * This function sets up and initializes the LVGL display driver, preparing it
 * for rendering graphical content. It must be called before using any LVGL
 * display-related functionality.
 *
 * @return
 *     - ESP_OK: Initialization was successful.
 *     - Appropriate error code from esp_err_t if initialization fails.
 */
esp_err_t lvgl_display_init(void);

/**
 * @brief Displays the pressure value on the LVGL display.
 *
 * This function updates the pressure label on the LVGL display with the given pressure value
 * and the full-scale (FS) value for reference.
 *
 * @param pressure The pressure value to be displayed (in mbar).
 * @param FS The full-scale value for the pressure measurement (used for scaling or reference).
 */
void lvgl_display_pressure(float pressure, float FS);


/**
 * @brief Displays the temperature on the LVGL display.
 *
 * This function updates the LVGL display to show the given temperature value.
 *
 * @param temp The temperature value to be displayed, in degrees Celsius.
 */
void lvgl_display_temperature(float temp);

/**
 * @brief Displays the given IP address on the LVGL display.
 *
 * This function takes an IP address as a string and renders it on the
 * display using the LVGL library.
 *
 * @param ipaddr A null-terminated string representing the IP address to display.
 *               The string should be in standard IPv4 or IPv6 format.
 */
void lvgl_display_ipaddr(const char* ipaddr);


/**
 * @brief Displays the internal temperature on the LVGL display.
 *
 * This function is used to update the LVGL display with the provided
 * temperature value. The temperature is expected to be >= -273.15°C.
 *
 * @param temp A float representing the temperature
 *             to be displayed (e.g., 25.3f).
 */
void lvgl_display_internal_temp(float temp);

/**
 * @brief Sets the display buffer percentage for the LVGL display.
 *
 * This function displays the fill percentage of the pressure measurement buffer.
 *
 * @param buf_pc The buffer percentage to display.
 */
void lvgl_display_buffer_pc(int buf_pc);

/**
 * @brief Controls the backlight of the LVGL display.
 *
 * This function turns the backlight of the display on or off based on the
 * provided parameter.
 *
 * @param on Set to true to turn the backlight on, or false to turn it off.
 */
void lvgl_display_backlight(bool on);

#ifdef __cplusplus
}
#endif