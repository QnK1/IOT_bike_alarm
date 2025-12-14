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
#include "wifi_ap.h"
#include "blink_manager.h"
#include "button_monitor.h"


static const char *TAG = "MAIN";


void app_main(void)
{
    xTaskCreate(&button_monitor_task, "button_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    xTaskCreate(&blink_task, "blink_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());                      // Inicjalizacja stosu TCP/IP
    ESP_ERROR_CHECK(esp_event_loop_create_default());       // Inicjalizacja pętli zdarzeń
    
    ESP_LOGI(TAG, "EPS-WIFI-CONFIG");

    wifi_ap_init();
    
    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}