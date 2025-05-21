#include "time_manager.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h" // For event group to signal sync (optional, flag is simpler here)
#include <string.h> // For memset
#include "esp_timer.h"

static const char *TAG = "TimeManager";

static volatile bool s_time_synchronized = false;
static time_sync_user_cb_t s_user_sync_cb = NULL;

// SNTP synchronization callback
static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronized with NTP server. Current time: %s", ctime(&tv->tv_sec));
    s_time_synchronized = true;
    if (s_user_sync_cb) {
        s_user_sync_cb();
    }
}

esp_err_t time_manager_init(void) {
    if (esp_sntp_enabled()) {
        ESP_LOGI(TAG, "SNTP already initialized.");
        // If it's already initialized and we want to re-register our callback,
        // or ensure our settings, we might need to stop and re-init,
        // or ESP-IDF's SNTP allows re-setting callbacks/servers.
        // For simplicity, we assume init is called once.
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SNTP...");
    s_time_synchronized = false;

    // Set timezone to UTC. You can make this configurable if needed.
    // For other timezones, refer to POSIX timezone string formats.
    // Example: "EST5EDT,M3.2.0/2,M11.1.0/2" for US Eastern Time
    setenv("TZ", POSIX_TIMEZONE_STRING, 1);
    tzset(); // Apply the timezone setting

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // You can configure specific NTP servers or use the default pool.ntp.org
    esp_sntp_setservername(0, "pool.ntp.org"); 
    // esp_sntp_setservername(1, "time.google.com"); // Example of a secondary server

    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_set_sync_interval(60 * 60 * 1000); // Set sync interval (here, 60 minutes)

    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP initialization request sent. Waiting for network and synchronization...");
    // Time synchronization will happen in the background once network is available.
    return ESP_OK;
}

void time_manager_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing SNTP");
    esp_sntp_stop();
    s_time_synchronized = false;
    s_user_sync_cb = NULL;
}

bool time_manager_is_synced(void) {
    // sntp_get_sync_status() can also be used:
    // return sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
    return s_time_synchronized;
}

time_t time_manager_get_timestamp_s(void) {
    if (!s_time_synchronized) {
        ESP_LOGD(TAG, "Time not yet synchronized. Returning 0.");
        return 0;
    }
    time_t now;
    time(&now);
    return now;
}

esp_err_t time_manager_get_timeval(struct timeval *tv) {
    if (tv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_time_synchronized) {
        ESP_LOGD(TAG, "Time not yet synchronized for gettimeofday.");
        memset(tv, 0, sizeof(struct timeval));
        return ESP_ERR_INVALID_STATE;
    }
    if (gettimeofday(tv, NULL) == 0) {
        return ESP_OK;
    }
    // Should not happen if time is synced, but as a fallback
    ESP_LOGE(TAG, "gettimeofday failed even though time reported as synced.");
    memset(tv, 0, sizeof(struct timeval));
    return ESP_FAIL;
}

void time_manager_register_sync_callback(time_sync_user_cb_t cb) {
    s_user_sync_cb = cb;
}


time_t time_manager_get_timestamp_ms() {
    time_t timestamp_ms = 0; // Default to 0 if epoch time is not available
    struct timeval current_time_tv;

    if (time_manager_get_timeval(&current_time_tv) == ESP_OK) {
        timestamp_ms = (current_time_tv.tv_sec * 1000ULL) + (current_time_tv.tv_usec / 1000ULL);
    } else {
        timestamp_ms = esp_timer_get_time() / 1000; // Time since boot in ms
        //ESP_LOGW(TAG, "Epoch time not available, using time since boot for timestamp.");
    }

    return timestamp_ms;
}