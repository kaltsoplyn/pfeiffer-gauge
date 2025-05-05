#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the web server (AP-STA mode)
 */
esp_err_t start_web_server(void);

/**
 * @brief Stop the currently running web server
 */
esp_err_t stop_web_server(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H



// #ifndef WEB_SERVER_H
// #define WEB_SERVER_H

// #include "esp_http_server.h"
// #include "esp_err.h"

// #ifdef __cplusplus
// extern "C" {
// #endif

// /**
//  * @brief Start the configuration web server (AP mode)
//  */
// void start_config_server(void);

// /**
//  * @brief Start the data web server (STA mode)
//  */
// void start_data_server(void);

// /**
//  * @brief Stop the currently running web server
//  */
// void stop_web_server(void);

// /**
//  * @brief Set callback for device data retrieval
//  * @param cb Callback function that returns JSON string
//  */
// void web_server_set_data_callback(const char* (*cb)(void));

// /**
//  * @brief Get the current server handle
//  * @return httpd_handle_t or NULL if no server running
//  */
// httpd_handle_t web_server_get_handle(void);

// #ifdef __cplusplus
// }
// #endif

// #endif // WEB_SERVER_H