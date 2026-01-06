#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mpu6050.h"
#include "arming_manager.h"

#include "config.h"

static const char *TAG = "MPU_MON";

void mpu_monitor_task(void *pvParameter)
{
    // Initialize MPU
    mpu6050_config_t mpu_config = {
        .scl_io = MPU_SCL_IO,
        .sda_io = MPU_SDA_IO,
        .device_addr = MPU_DEVICE_ADDR,
        .i2c_port = MPU_I2C_PORT
    };

    if (mpu6050_init(&mpu_config) != ESP_OK) {
        ESP_LOGE(TAG, "MPU Init Failed! Task deleting.");
        vTaskDelete(NULL);
    }
    
    // Initial calibration
    mpu6050_set_accel_range(ACCEL_RANGE_4G);
    
    bool motion_mode_active = false;

    while (1) {
        
        // If armed and not in alaram, enable motion detection
        if (is_system_armed() && !is_system_in_alarm()) {
            
            if (!motion_mode_active) {
                ESP_LOGI(TAG, "Configuring MPU for Motion Detection...");
                mpu6050_enable_motion_detection(20, 1);
                motion_mode_active = true;
                mpu6050_get_int_status(); 
            }
            uint8_t status = mpu6050_get_int_status();
            if (status & 0x40) {
                ESP_LOGE(TAG, "Motion Detected! (Status: 0x%02X)", status);
                trigger_system_alarm();
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        } 
        // Is not armed or alarm already runnning
        else {
            if (motion_mode_active) {
                ESP_LOGI(TAG, "Disabling Motion Detection (Normal Mode)");
                mpu6050_set_normal_mode();
                motion_mode_active = false;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}