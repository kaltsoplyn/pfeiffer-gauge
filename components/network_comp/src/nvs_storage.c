#include "nvs_storage.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "NVSStorage";
static nvs_handle_t nvs_hndl;

esp_err_t nvs_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition corrupted or new version found, erasing...");
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_open("storage", NVS_READWRITE, &nvs_hndl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "NVS initialized");
    return ESP_OK;
}

esp_err_t nvs_store_wifi_creds(const char *ssid, const char *password) {
    esp_err_t err = nvs_set_str(nvs_hndl, "wifi_ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set SSID in NVS: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_str(nvs_hndl, "wifi_pass", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Password in NVS: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_commit(nvs_hndl);
    if (err == ESP_OK) ESP_LOGI(TAG, "WiFi credentials stored"); else ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
    return err;
}

bool nvs_get_wifi_creds(char *ssid, size_t ssid_len, char *password, size_t password_len) {
    size_t required_ssid_len = ssid_len;
    size_t required_password_len = password_len;

    if (nvs_get_str(nvs_hndl, "wifi_ssid", ssid, &required_ssid_len) != ESP_OK) {
        return false;
    }
    if (nvs_get_str(nvs_hndl, "wifi_pass", password, &required_password_len) != ESP_OK) {
        return false;
    }
    // Add check: If password length retrieved is 0, treat as invalid credentials for non-open networks
    if (required_password_len == 0) {
        ESP_LOGW(TAG, "Retrieved empty password from NVS.");
        return false;
    }
    return true;
}

esp_err_t nvs_erase_wifi_creds(void) {
    esp_err_t err1 = nvs_erase_key(nvs_hndl, "wifi_ssid");
    // ESP_ERR_NVS_NOT_FOUND is okay here, means it was already gone
    if (err1 != ESP_OK && err1 != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase SSID: %s", esp_err_to_name(err1));
        // Decide if you want to return early or still try to erase password/commit
    }
    esp_err_t err2 = nvs_erase_key(nvs_hndl, "wifi_pass");
    if (err2 != ESP_OK && err2 != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase Password: %s", esp_err_to_name(err2));
    }
    esp_err_t commit_err = nvs_commit(nvs_hndl);
    if (commit_err == ESP_OK) ESP_LOGI(TAG, "WiFi credentials erased"); else ESP_LOGE(TAG, "NVS commit failed after erase: %s", esp_err_to_name(commit_err));
    return commit_err; // Return the status of the commit operation
}