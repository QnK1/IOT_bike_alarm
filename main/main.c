#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "wifi.h"
#include "wifi_ap.h"
#include "blink_manager.h"
#include "button_monitor.h"
#include "arming_manager.h"
#include "mpu_monitor.h"
#include "alarm_runner.h"
#include "gps.h"
#include "battery.h"
#include "mqtt_cl.h"
#include "ble_config.h"

static const char *TAG = "MAIN";

#define NVS_NAMESPACE "storage"
#define KEY_FORCE_CONFIG "force_conf"

// Helper to check if we should start in config mode
bool should_enter_config_mode(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return true; // Error -> Default to config

    // 1. Check for Force Config Flag (set by button)
    uint8_t force_config = 0;
    nvs_get_u8(handle, KEY_FORCE_CONFIG, &force_config);
    if (force_config == 1) {
        ESP_LOGW(TAG, "Force Config Flag Detected");
        // Clear the flag so next boot is normal if config succeeds
        nvs_set_u8(handle, KEY_FORCE_CONFIG, 0);
        nvs_commit(handle);
        nvs_close(handle);
        return true;
    }

    // 2. Check for User ID
    size_t required_size = 0;
    esp_err_t err_user = nvs_get_str(handle, "user_id", NULL, &required_size);
    bool has_user = (err_user == ESP_OK && required_size > 0);

    // 3. Check for WiFi Creds
    esp_err_t err_ssid = nvs_get_str(handle, "wifi_ssid", NULL, &required_size);
    bool has_wifi = (err_ssid == ESP_OK && required_size > 0);

    nvs_close(handle);

    if (!has_user) ESP_LOGW(TAG, "Missing User ID");
    if (!has_wifi) ESP_LOGW(TAG, "Missing WiFi Credentials");

    return (!has_user || !has_wifi);
}

void app_main(void)
{
    // 1. Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Init Netif & Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Start Button Monitor (Always needed for reset/config entry)
    xTaskCreate(&button_monitor_task, "button", 2048, NULL, 5, NULL);
    // Start Blink Manager (Visual feedback)
    xTaskCreate(&blink_task, "blink", 2048, NULL, 5, NULL);

    // 4. Decide Mode
    if (should_enter_config_mode()) {
        ESP_LOGW(TAG, ">>> ENTERING CONFIGURATION MODE (BLE + AP) <<<");
        
        // Start BLE for User Assignment
        ble_config_init();

        // Start WiFi Access Point for SSID/Pass
        wifi_start_ap_mode();

        // In config mode, we do NOT start sensors, MQTT, or GPS to save power
        // The device waits here indefinitely until user resets via AP save or Button
    } 
    else {
        ESP_LOGI(TAG, ">>> ENTERING NORMAL MODE (UNARMED) <<<");

        ble_config_deinit();
        
        arming_init();

        // Load WiFi and Connect
        char ssid[32] = {0}, pass[64] = {0};
        load_wifi_credentials(ssid, pass); // From wifi_ap.h/c
        wifi_init_sta(ssid, pass);

        // Init Hardware
        gps_init(); 
        battery_init();
        
        // Start Tasks
        xTaskCreate(&battery_monitor_task, "bat_mon", 2048, NULL, 1, NULL);
        xTaskCreate(&mpu_monitor_task, "mpu_mon", 4096, NULL, 5, NULL);
        xTaskCreate(&alarm_runner_task, "alarm_run", 4096, NULL, 5, NULL);
        
        mqtt_app_start();
    }
}