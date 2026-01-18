#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "wifi.h"
#include "ble_config.h" // Changed include
#include "button_monitor.h"
#include "arming_manager.h"

#define LED_PIN 2

void blink_task(void *pvParameter) 
{
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    while(1)
    {
        // PRIORITY 1: ALARM ACTIVE (Panic Strobe)
        if (is_system_in_alarm()) {
            // Fast strobe: 50ms ON, 50ms OFF
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // PRIORITY 2: SYSTEM RESTARTING
        if (esp_is_restarting()){
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(60));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(60));
            continue;
        }

        // PRIORITY 3: ARMED MODE (Stealth / Heartbeat)
        if (is_system_armed()) {
            if (wifi_is_connected()) {
                // Heartbeat
                gpio_set_level(LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(50));
                gpio_set_level(LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(50));
                gpio_set_level(LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(2000));
            } else {
                // Stealth Mode
                gpio_set_level(LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(20));
                gpio_set_level(LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(5000)); 
            }
        }
        // PRIORITY 4: UNARMED
        else {
            // Changed Logic: Check BLE Config Active
            if (ble_config_is_active()) 
            {
                // Config Mode: Long ON sequence to indicate "Connect via BLE"
                gpio_set_level(LED_PIN, 1);
                // Wait 1.2s, but check status frequently
                for(int i = 0; i < 12; i++) {
                    if(!ble_config_is_active() || is_system_armed()) break;
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                
                gpio_set_level(LED_PIN, 0);
                for(int i = 0; i < 2; i++) {
                    if(!ble_config_is_active() || is_system_armed()) break;
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            }
            else if (wifi_is_connected())
            {
                // STA Connected: 200ms ON, 200ms OFF (Slow Blink)
                gpio_set_level(LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
            else
            {
                // STA Connecting / Idle
                gpio_set_level(LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
    }
}