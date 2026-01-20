#ifndef NVS_STORE_H
#define NVS_STORE_H

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

// Keys used in NVS
#define KEY_USER_ID "user_id"
#define KEY_SSID    "wifi_ssid"
#define KEY_PASS    "wifi_pass"
#define KEY_FORCE_CONFIG "force_conf"
#define KEY_DEVICE_ID    "device_id"

// General NVS Helper
esp_err_t nvs_store_init(void);

// User ID
bool nvs_has_user_id(void);
esp_err_t nvs_save_user_id(const char* user_id);
esp_err_t nvs_load_user_id(char* buffer, size_t max_len);

// Device ID
bool nvs_has_device_id(void);
esp_err_t nvs_save_device_id(const char* device_id);
esp_err_t nvs_load_device_id(char* buffer, size_t max_len);

// WiFi Credentials
bool nvs_has_wifi_creds(void);
esp_err_t nvs_save_wifi_creds(const char* ssid, const char* pass);
esp_err_t nvs_load_wifi_creds(char* ssid_buf, size_t ssid_len, char* pass_buf, size_t pass_len);

// Force Config Flag
bool nvs_get_force_config(void);
void nvs_clear_force_config(void);
void nvs_set_force_config(void);

#endif // NVS_STORE_H