#include "nvs_store.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_STORE";
#define NVS_NAMESPACE "storage"

esp_err_t nvs_store_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t save_str(const char* key, const char* value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t load_str(const char* key, char* buffer, size_t max_len) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    err = nvs_get_str(handle, key, buffer, &max_len);
    nvs_close(handle);
    return err;
}

// --- User ID ---

bool nvs_has_user_id(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;
    size_t required_size;
    esp_err_t err = nvs_get_str(handle, KEY_USER_ID, NULL, &required_size);
    nvs_close(handle);
    return (err == ESP_OK && required_size > 0);
}

esp_err_t nvs_save_user_id(const char* user_id) {
    return save_str(KEY_USER_ID, user_id);
}

esp_err_t nvs_load_user_id(char* buffer, size_t max_len) {
    return load_str(KEY_USER_ID, buffer, max_len);
}

// --- WiFi Credentials ---

bool nvs_has_wifi_creds(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;
    size_t sz_ssid;
    esp_err_t err = nvs_get_str(handle, KEY_SSID, NULL, &sz_ssid);
    nvs_close(handle);
    return (err == ESP_OK && sz_ssid > 0);
}

esp_err_t nvs_save_wifi_creds(const char* ssid, const char* pass) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, KEY_SSID, ssid);
    if (err == ESP_OK) err = nvs_set_str(handle, KEY_PASS, pass);
    if (err == ESP_OK) err = nvs_commit(handle);

    nvs_close(handle);
    return err;
}

esp_err_t nvs_load_wifi_creds(char* ssid_buf, size_t ssid_len, char* pass_buf, size_t pass_len) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    err = nvs_get_str(handle, KEY_SSID, ssid_buf, &ssid_len);
    if (err == ESP_OK) err = nvs_get_str(handle, KEY_PASS, pass_buf, &pass_len);

    nvs_close(handle);
    return err;
}

// --- Force Config Flag ---

bool nvs_get_force_config(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;
    uint8_t val = 0;
    nvs_get_u8(handle, KEY_FORCE_CONFIG, &val);
    nvs_close(handle);
    return (val == 1);
}

void nvs_clear_force_config(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, KEY_FORCE_CONFIG, 0);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

void nvs_set_force_config(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, KEY_FORCE_CONFIG, 1);
        nvs_commit(handle);
        nvs_close(handle);
    }
}