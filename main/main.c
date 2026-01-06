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

void app_main(void)
{
    arming_init();

    // Start core tasks
    xTaskCreate(&button_monitor_task, "button", 2048, NULL, 5, NULL);
    xTaskCreate(&blink_task, "blink", 2048, NULL, 5, NULL);
    
    // Start sensor and alarm tasks
    xTaskCreate(&mpu_monitor_task, "mpu_mon", 4096, NULL, 5, NULL);
    xTaskCreate(&alarm_runner_task, "alarm_run", 2048, NULL, 5, NULL);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    mpu6050_data_t sensor_data;
    mpu6050_offsets_t offsets;

    mpu6050_config_t my_mpu_config = {
        .scl_io = 22,
        .sda_io = 21,
        .device_addr = 0x68,
        .i2c_port = I2C_MASTER_NUM_0
    };

    // --- 1. INICJALIZACJA I TEST POŁĄCZENIA ---
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "ETAP 1: Inicjalizacja i Test Połączenia");
    ESP_LOGI(TAG, "=================================================");
    
    ESP_ERROR_CHECK(mpu6050_init(&my_mpu_config));

    if (mpu6050_test_connection()) {
        ESP_LOGI(TAG, "MPU-6050 wykryty poprawnie! Adres: 0x68.");
    }
    else {
        ESP_LOGE(TAG, "Nie znaleziono MPU-6050!");
        return;
    }

    // --- 2. KONFIGURACJA ZAKRESÓW I FILTRÓW ---
    ESP_LOGI(TAG, "\n=================================================");
    ESP_LOGI(TAG, "ETAP 2: Konfiguracja (set_accel_range, set_dlpf_mode)");
    ESP_LOGI(TAG, "=================================================");
    
    // Ustawienie Zakresów: set_accel_range, set_gyro_range
    ESP_ERROR_CHECK(mpu6050_set_accel_range(ACCEL_RANGE_4G));
    ESP_LOGI(TAG, "Ustawiono zakres Akcelerometru na: 4G");
    ESP_ERROR_CHECK(mpu6050_set_gyro_range(GYRO_RANGE_1000DPS));
    ESP_LOGI(TAG, "Ustawiono zakres Żyroskopu na: 1000 DPS");

    // Ustawienie Filtra: set_dlpf_mode
    ESP_ERROR_CHECK(mpu6050_set_dlpf_mode(DLPF_5HZ));
    ESP_LOGI(TAG, "Ustawiono Filtr Cyfrowy (DLPF) na: 5 Hz (MAX Wygładzenie)");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_ap_init(); 
}