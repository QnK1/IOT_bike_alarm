#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "wifi.h"
#include "nvs_store.h" // Replaces wifi_ap.h
#include "blink_manager.h"
#include "button_monitor.h"
#include "arming_manager.h"
#include "mpu_monitor.h"
#include "alarm_runner.h"
#include "gps.h"
#include "battery.h"
#include "mqtt_cl.h"
#include "ble_config.h"
#include "lora.h"

static const char *TAG = "MAIN";

// Helper to check if we should start in config mode
bool should_enter_config_mode(void) {
    // 1. Check for Force Config Flag
    if (nvs_get_force_config()) {
        ESP_LOGW(TAG, "Force Config Flag Detected");
        nvs_clear_force_config();
        return true;
    }

    // 2. Check for User ID & Wifi
    bool has_user = nvs_has_user_id();
    bool has_wifi = nvs_has_wifi_creds();

    if (!has_user) ESP_LOGW(TAG, "Missing User ID");
    if (!has_wifi) ESP_LOGW(TAG, "Missing WiFi Credentials");

    return (!has_user || !has_wifi);
}

void app_main(void)
{
    // 1. Init NVS
    ESP_ERROR_CHECK(nvs_store_init());

    // 2. Init Netif & Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Start Monitors
    xTaskCreate(&button_monitor_task, "button", 5120, NULL, 5, NULL);
    xTaskCreate(&blink_task, "blink", 2048, NULL, 5, NULL);

    ESP_ERROR_CHECK(lora_init());

    // 4. Decide Mode
    if (should_enter_config_mode()) {
        ESP_LOGW(TAG, ">>> ENTERING CONFIGURATION MODE (BLE ONLY) <<<");
        
        // Start BLE for User Assignment AND WiFi Provisioning
        ble_config_init();

        // Device waits here. User connects via BLE, sets UserID, SSID, PASS, 
        // and sends "Action=1" to save and reboot.
    } 
    else {
        ESP_LOGI(TAG, ">>> ENTERING NORMAL MODE (UNARMED) <<<");

        ble_config_deinit();
        
        arming_init();

        // Load WiFi and Connect
        char ssid[32] = {0}, pass[64] = {0};
        nvs_load_wifi_creds(ssid, 32, pass, 64);
        wifi_init_sta(ssid, pass);

        // Init Hardware
        gps_init(); 
        battery_init();
        
        // Start Tasks
        xTaskCreate(&battery_monitor_task, "bat_mon", 5120, NULL, 1, NULL);
        xTaskCreate(&mpu_monitor_task, "mpu_mon", 4096, NULL, 5, NULL);
        xTaskCreate(&alarm_runner_task, "alarm_run", 4096, NULL, 5, NULL);
    }
}