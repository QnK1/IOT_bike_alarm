#include <string.h>
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

static const char *TAG = "MAIN";


void app_main(void)
{
    
    xTaskCreate(&blink_task, "blink_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    
    ESP_LOGI(TAG, "ESP-WIFI-STATION");

    
    wifi_init_sta();

    mqtt_app_start();
    
    xTaskCreate(&http_get_task, "http_get_task", 8192, NULL, 5, NULL);

    
    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}