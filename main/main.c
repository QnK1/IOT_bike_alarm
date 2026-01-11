#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "mqtt_cl.h"
#include "wifi.h"
#include "wifi_ap.h"
#include "blink_manager.h"
#include "button_monitor.h"
#include "arming_manager.h"
#include "mpu_monitor.h"
#include "alarm_runner.h"
#include "gps.h"
#include "battery.h"

void app_main(void)
{
    wifi_ap_init(); 
    mqtt_app_start();
    
    arming_init();

    // Start core tasks
    xTaskCreate(&button_monitor_task, "button", 2048, NULL, 5, NULL);
    xTaskCreate(&blink_task, "blink", 2048, NULL, 5, NULL);

    // Start sensor and alarm tasks
    xTaskCreate(&mpu_monitor_task, "mpu_mon", 4096, NULL, 5, NULL);
    xTaskCreate(&alarm_runner_task, "alarm_run", 2048, NULL, 5, NULL);
    xTaskCreate(&alarm_runner_task, "alarm_run", 4096, NULL, 5, NULL);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    gps_init(); 
    
    // --- Battery Init ---
    battery_init();
    xTaskCreate(&battery_monitor_task, "bat_mon", 2048, NULL, 1, NULL); // Low priority (1)
}