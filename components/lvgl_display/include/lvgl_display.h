#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PRESSURE_GAUGE_NAME "Pfeiffer CMR362"

/**
 * @brief Initializes the LVGL display.
 *
 * This function sets up and configures the LVGL display for use. It should
 * be called during the initialization phase of the application to ensure
 * the display is properly prepared for rendering graphical content.
 */
void lvgl_display_init(void);

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
 * This function is responsible for rendering the internal temperature
 * information on the LVGL display. It can also display the provided
 * IP address as part of the information.
 *
 * @param ipaddr A pointer to a null-terminated string containing the IP address
 *               to be displayed. If no IP address is available, this parameter
 *               can be set to NULL or an empty string.
 */
void lvgl_display_internal_temp(const char* temp);

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