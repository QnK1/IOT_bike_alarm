#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "wifi.h"
#include "wifi_ap.h"
#include "button_monitor.h"

#define LED_PIN 2
// static const char *TAG = "blink manager";

void blink_task(void *pvParameter) 
{
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    while(1)
    {
        if (esp_is_restarting()){
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(60));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(60));
        }
        else if (wifi_is_ap_active()) 
        {
            gpio_set_level(LED_PIN, 1);
            for(int i = 0; i < 12 && wifi_is_ap_active(); i++)
                vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_PIN, 0);
            for(int i = 0; i < 2 && wifi_is_ap_active(); i++)
                vTaskDelay(pdMS_TO_TICKS(100));
        }
        else if (wifi_is_connected())
        {
            gpio_set_level(LED_PIN, 1);
            for(int i = 0; i < 2 && wifi_is_connected(); i++)
                vTaskDelay(pdMS_TO_TICKS(100));
        }
        else
        {
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}