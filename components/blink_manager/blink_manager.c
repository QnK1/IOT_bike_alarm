#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "wifi.h"
#include "wifi_ap.h"
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

        // PRIORITY 2: SYSTEM RESTARTING (Visual Confirmation)
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
                
                // Wait 2 seconds
                vTaskDelay(pdMS_TO_TICKS(2000));
            } else {
                // Stealth Mode (WiFi lost/sleeping):
                // Very short blip every 5 seconds
                gpio_set_level(LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(20));
                gpio_set_level(LED_PIN, 0);
                
                vTaskDelay(pdMS_TO_TICKS(5000)); 
            }
        }
        // PRIORITY 4: UNARMED (Standard Configuration)
        else {
            if (wifi_is_ap_active()) 
            {
                // AP Mode: Long ON sequence to indicate "Config Me"
                gpio_set_level(LED_PIN, 1);
                // Wait 1.2s, but check status frequently to react to mode changes
                for(int i = 0; i < 12; i++) {
                    if(!wifi_is_ap_active() || is_system_armed()) break;
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                
                gpio_set_level(LED_PIN, 0);
                for(int i = 0; i < 2; i++) {
                    if(!wifi_is_ap_active() || is_system_armed()) break;
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            }
            else if (wifi_is_connected())
            {
                // STA Connected: 200ms ON, 200ms OFF (Slow Blink)
                // Or Solid ON if preferred, but blink verifies loop is running
                gpio_set_level(LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(2000)); // Long pause = Healthy connection
            }
            else
            {
                // STA Connecting / Idle: Regular 200ms Toggle
                gpio_set_level(LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
    }
}