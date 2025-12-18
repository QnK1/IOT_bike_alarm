#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_rom_sys.h"

#include "wifi.h"
#include "http.h"
#include "mqtt_cl.h"
#include "mpu6050.h" 

static const char *TAG = "MAIN";


void app_main(void)
{
    // xTaskCreate(&blink_task, "blink_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);

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


    // --- 3. KALIBRACJA ---
    ESP_LOGI(TAG, "\n=================================================");
    ESP_LOGI(TAG, "ETAP 3: Kalibracja (mpu6050_calibrate)");
    ESP_LOGW(TAG, "PROSZĘ POŁOŻYĆ URZĄDZENIE PŁASKO. Kalibracja za 5 sekund...");
    ESP_LOGI(TAG, "=================================================");
    
    vTaskDelay(pdMS_TO_TICKS(5000));
    mpu6050_calibrate(200);
    
    offsets = mpu6050_get_offsets();
    
    mpu6050_set_normal_mode();

    // --- 4. PREZENTACJA TRYBU POMIAROWEGO (Tryb Normalny) ---
    ESP_LOGI(TAG, "\n=================================================");
    ESP_LOGI(TAG, "ETAP 4: Tryb Pomiaru Ciągłego (20 sekund)");
    ESP_LOGI(TAG, "=================================================");
    
    int pomiary = 0;
    while(pomiary < 40) {
        if (mpu6050_get_data(&sensor_data) == ESP_OK) {
             ESP_LOGI(TAG, "Accel: X=%.2f g, Y=%.2f g, Z=%.2f g | Gyro: X=%.2f deg/s, Y=%.2f deg/s, Z=%.2f deg/s | Temp: %.2f C",
                     sensor_data.ax, sensor_data.ay, sensor_data.az,
                     sensor_data.gx, sensor_data.gy, sensor_data.gz, sensor_data.temp);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        pomiary++;
    }


    // --- 5. PREZENTACJA TRYBU CZUWANIA (Motion Detection) ---
    ESP_LOGI(TAG, "\n=================================================");
    ESP_LOGW(TAG, "ETAP 5: Tryb Low-Power Motion Detect (Alarm)");
    ESP_LOGI(TAG, "=================================================");

    mpu6050_enable_motion_detection(20, 1);
    ESP_LOGW(TAG, "Tryb Motion Detection WŁĄCZONY.");
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t initial_status = mpu6050_get_int_status();


    int alarm_test_duration = 200;
    while(alarm_test_duration > 0) {
        uint8_t status = mpu6050_get_int_status();
        
        if (status & 0x40) {
            ESP_LOGE(TAG, ">>> ALARM RUCHU! <<< Status 0x%02X", status);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        alarm_test_duration--;
    }
    
    ESP_LOGI(TAG, "Koniec testu alarmowego.");


    // --- 6. POWRÓT DO TRYBU NORMALNEGO I ZAKOŃCZENIE ---
    ESP_LOGI(TAG, "\n=================================================");
    ESP_LOGI(TAG, "ETAP 6: Powrót do Pomiarów");
    ESP_LOGI(TAG, "=================================================");

    mpu6050_set_normal_mode(); 
    ESP_LOGI(TAG, "Przywrócono pełny tryb pomiarowy (Gyro ON).");

    
    // Pomiary w nieskończonej pętli
    while(1) {
        if (mpu6050_get_data(&sensor_data) == ESP_OK) {
             ESP_LOGI(TAG, "Accel: X=%.2f g, Y=%.2f g, Z=%.2f g | Gyro: X=%.2f deg/s, Y=%.2f deg/s, Z=%.2f deg/s | Temp: %.2f C",
                     sensor_data.ax, sensor_data.ay, sensor_data.az,
                     sensor_data.gx, sensor_data.gy, sensor_data.gz, sensor_data.temp);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}