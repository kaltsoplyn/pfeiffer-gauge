#include "wifi_manager.h"
#include "web_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_storage.h"
#include "esp_wifi.h"  // Required for wifi_ap_record_t
#include "pressure_meas_comp.h"

static const char *TAG = "WebServer";
static httpd_handle_t server = NULL;


// Helper function to convert a hex character to its integer value
static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1; // Invalid hex char
}

// Basic URL decoding function
// Decodes src into dest. dest_len is IN/OUT parameter.
// Returns ESP_OK on success, ESP_FAIL on error (e.g., invalid encoding, buffer too small)
static esp_err_t custom_uri_decode(char *dest, const char *src, size_t *dest_len) {
    size_t max_dest_len = *dest_len;
    size_t dest_idx = 0;
    const char *p = src;

    while (*p && dest_idx < max_dest_len -1) { // -1 to leave space for null terminator
        if (*p == '%') {
            p++; // Move to the first hex digit
            if (!*p) return ESP_FAIL; // Truncated encoding
            int high = hex_char_to_int(*p);
            if (high == -1) return ESP_FAIL; // Invalid hex

            p++; // Move to the second hex digit
            if (!*p) return ESP_FAIL; // Truncated encoding
            int low = hex_char_to_int(*p);
            if (low == -1) return ESP_FAIL; // Invalid hex

            dest[dest_idx++] = (char)((high << 4) | low);
            p++; // Move past the second hex digit
        } else if (*p == '+') { // Treat '+' as space (common in form data)
            dest[dest_idx++] = ' ';
            p++;
        } else {
            dest[dest_idx++] = *p;
            p++;
        }
    }

     if (*p != '\0' && dest_idx >= max_dest_len -1) {
         // Source string not fully processed, destination buffer too small
         *dest_len = dest_idx; // Report how much was written
         return ESP_ERR_INVALID_SIZE;
     }

    dest[dest_idx] = '\0'; // Null-terminate the destination string
    *dest_len = dest_idx; // Update dest_len to actual length written
    return ESP_OK;
}



// Raw HTML strings
static const char* CONFIG_HTML = 
"<!DOCTYPE html>"
    "<html>"
        "<head>"
            "<title>WiFi Config</title>"
            "<style>"
                "body{font-family:Arial,sans-serif;margin:20px}"
                "table{border-collapse:collapse;width:600px}"
                "td,th{padding:8px;text-align:left;border-bottom:1px solid #ddd}"
                "button{padding:5px 10px;background:#4CAF50;color:white;border:none;border-radius:3px}"
                "input{padding:5px;width:200px}"
            "</style>"
        "</head>"
        "<body>"
            "<h1>Available Networks (reload page to rescan)</h1>"
                "<table>"
                   "<tr><th>SSID</th><th>Password</th><th></th></tr>" // Added header row
                   "%s" // Placeholder for table rows
                "</table>"
        "<script>"
            "function connect(idx, ssid){" // Accept index and ssid
            "  var pwd=document.getElementById('pwd_'+idx).value;" // Use index for ID
            "  window.location.href='/connect?ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pwd);"
            "}"
        "</script>"
"</html>";

static const char* DATA_HTML = 
"<!DOCTYPE html>"
    "<html>"
        "<head>"
            "<title>Device Data</title>"
            "<style>"
                "body{font-family:Arial,sans-serif;margin:20px}"
                "#data{padding:10px;background:#f5f5f5;border-radius:5px}"
            "</style>"
        "</head>"
        "<body>"
            "<h1>Device Data</h1>"
            "<div id='data'>Loading...</div>"
        "<script>"
            "fetch('/api/data').then(r=>r.json()).then(d=>{"
            "  document.getElementById('data').innerText=JSON.stringify(d,null,2);"
            "});"
        "</script>"
    "</body>"
"</html>";

// Handler for WiFi config page
static esp_err_t config_handler(httpd_req_t *req) {
    #define MAX_APs 10
    wifi_ap_record_t ap_info[MAX_APs];
    uint16_t ap_count = MAX_APs;
    esp_err_t ret = ESP_OK;

    // Perform WiFi scan
    ret = wifi_scan_networks(ap_info, &ap_count);  // Use return value for error check
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed in config_handler: %s", esp_err_to_name(ret));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    if (ap_count == 0) {
        ESP_LOGW(TAG, "No networks found during scan.");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Estimate buffer size for table rows HTML (generous estimate: 150 chars per AP)
    size_t table_rows_max_len = ap_count * 200 + 1; // +1 for null terminator
    char *table_rows_html = malloc(table_rows_max_len);
    if (!table_rows_html) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for table rows", table_rows_max_len);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    table_rows_html[0] = '\0'; // Start with an empty string
    char *current_pos = table_rows_html;
    size_t remaining_len = table_rows_max_len;

    // Build network table rows HTML
    for (int i = 0; i < ap_count; i++) {
        int written = snprintf(current_pos, remaining_len,
                             "<tr><td>%.32s</td><td><input type='password' id='pwd_%d' placeholder='Password'></td>" // Use index for ID
                             "<td><button onclick='connect(%d, \"%.32s\")'>Connect</button></td></tr>", // Pass index and SSID to JS
                             ap_info[i].ssid, i, i, ap_info[i].ssid);
        if (written >= remaining_len) {
             ESP_LOGW(TAG, "Table rows buffer potentially truncated");
             break; // Avoid buffer overflow
        }
        current_pos += written;
        remaining_len -= written;
    }

    // Calculate final HTML size and allocate buffer
    size_t final_html_len = strlen(CONFIG_HTML) + strlen(table_rows_html) + 1; // +1 for null terminator
    char *final_html = malloc(final_html_len);
    if (!final_html) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for final HTML", final_html_len);
        free(table_rows_html); // Clean up previously allocated memory
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Format the final HTML
    snprintf(final_html, final_html_len, CONFIG_HTML, table_rows_html);

    // Send response and free memory
    ret = httpd_resp_send(req, final_html, HTTPD_RESP_USE_STRLEN);
    free(table_rows_html);
    free(final_html);

    return ret;
}

// Handler for connect request
static esp_err_t connect_handler(httpd_req_t *req) {
    char query[100] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get query string");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Connect query: %s", query);

    // Buffers for raw and decoded values
    char ssid_raw[64] = {0}; // Increased size for potential encoding expansion
    char password_raw[128] = {0};
    char ssid_decoded[32] = {0};
    char password_decoded[64] = {0};
    size_t decoded_len;

    // Get raw values
    httpd_query_key_value(query, "ssid", ssid_raw, sizeof(ssid_raw));
    httpd_query_key_value(query, "password", password_raw, sizeof(password_raw));

    // Decode SSID
    decoded_len = sizeof(ssid_decoded); // httpd_uri_decode needs length IN and provides length OUT
    if (custom_uri_decode(ssid_decoded, ssid_raw, &decoded_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode SSID");
        httpd_resp_send_500(req); return ESP_FAIL;
    }
    // Decode Password
    decoded_len = sizeof(password_decoded);
    if (custom_uri_decode(password_decoded, password_raw, &decoded_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode Password");
        httpd_resp_send_500(req); return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Received connect request for SSID: '%s'", ssid_decoded);

    // 1. Store credentials in NVS
    esp_err_t nvs_err = nvs_store_wifi_creds(ssid_decoded, password_decoded);
    if (nvs_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store WiFi credentials in NVS: %s", esp_err_to_name(nvs_err));
        httpd_resp_send_500(req); // Send an error response
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Credentials stored successfully.");

    // 2. Initiate connection attempt (this function should handle mode switching and connection logic)
    wifi_connect_sta(ssid_decoded, password_decoded); // Use decoded values

    // 3. Send response to the client
    // Send an HTTP 302 Redirect to the /data page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/data");
    // Send empty body for redirect
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

// Handler for data page
static esp_err_t data_handler(httpd_req_t *req) {
    httpd_resp_send(req, DATA_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler for data API
static esp_err_t api_data_handler(httpd_req_t *req) {
    // Buffer to hold data retrieved from the component
    PressureData data_buffer[PRESSURE_BUFFER_SIZE]; // Use the size defined in pressure_meas_comp.h
    int data_count = pressure_meas_get_buffered_data(data_buffer, PRESSURE_BUFFER_SIZE);

    // Estimate required JSON buffer size:
    // Approx 50 chars per entry {"p":123.45,"t":12345.678}, + overhead
    size_t json_buffer_size = (data_count * 50) + 50; // +50 for base structure and safety
    char *json_buffer = malloc(json_buffer_size);
    if (!json_buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer for JSON response (%d bytes)", json_buffer_size);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Build the JSON string
    char *ptr = json_buffer;
    size_t remaining_len = json_buffer_size;
    int written;

    // Start JSON object and data array
    written = snprintf(ptr, remaining_len, "{\"status\":\"ok\",\"count\":%d,\"data\":[", data_count);
    ptr += written;
    remaining_len -= written;

    // Add each data point
    for (int i = 0; i < data_count && remaining_len > 1; i++) {
        written = snprintf(ptr, remaining_len, "%s{\"p\":%.2f,\"t\":%.3f}",
                         (i > 0 ? "," : ""), // Add comma separator
                         data_buffer[i].pressure,
                         data_buffer[i].timestamp);
        if (written >= remaining_len) {
            ESP_LOGW(TAG, "JSON buffer potentially truncated");
            // Consider sending partial data or an error
            break;
        }
        ptr += written;
        remaining_len -= written;
    }

    // Close array and object
    snprintf(ptr, remaining_len, "]}");

    // Send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buffer, HTTPD_RESP_USE_STRLEN);
    free(json_buffer); // Free allocated memory

    return ESP_OK;
}

esp_err_t start_web_server(void) {
    if (server) return ESP_OK;
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 12288;  // Increased from default 4096
    config.max_uri_handlers = 8;
    config.max_open_sockets = 4;

    esp_err_t ret = httpd_start(&server, &config);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server!\n%s", esp_err_to_name(ret));
        return ret;
    }
    
    httpd_uri_t config_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = config_handler,
    };
    
    httpd_uri_t connect_uri = {
        .uri = "/connect",
        .method = HTTP_GET,
        .handler = connect_handler,
    };

    httpd_uri_t data_uri = {
        .uri = "/data",
        .method = HTTP_GET,
        .handler = data_handler, // Serves the DATA_HTML page
    };
    
    httpd_uri_t api_uri = {
        .uri = "/api/data",
        .method = HTTP_GET,
        .handler = api_data_handler, // Serves the JSON data
    };

    
    ret = httpd_register_uri_handler(server, &config_uri); // Register data page handler
    ret = ret == ESP_OK ? httpd_register_uri_handler(server, &connect_uri) : ret;  // Register API handler
    ret = ret == ESP_OK ? httpd_register_uri_handler(server, &data_uri) : ret; // Register data page handler
    ret = ret == ESP_OK ? httpd_register_uri_handler(server, &api_uri) : ret;  // Register API handler
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register URI handlers!\n%s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Web server started on port %d", CONFIG_WEB_PORT); // Updated log message

    return ESP_OK;
}


esp_err_t stop_web_server(void) {
    if (!server) return ESP_OK;
    
    esp_err_t ret = httpd_stop(server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop the server!\n%s", esp_err_to_name(ret));
        return ret;
    }
    server = NULL;
    ESP_LOGI(TAG, "Web server stopped");
    
    return ESP_OK;
}