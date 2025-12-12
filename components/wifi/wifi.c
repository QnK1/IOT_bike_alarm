#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_rom_sys.h"

#include "wifi.h"

#define WIFI_SSID      "endzju"
#define WIFI_PASS      "royale123"

static const int retry_wait_time_ms = 2 * 1000;

static int s_retries_count = 0; 
static QueueHandle_t s_wifi_retry_event_queue;
    
static const char *TAG = "WIFI";

static EventGroupHandle_t wifi_event_group; 
#define WIFI_CONNECTED_BIT (1UL << 0)

bool wifi_is_connected(void) {
    if (wifi_event_group == NULL) {
        return false;
    }
    EventBits_t uxBits = xEventGroupGetBits(wifi_event_group);
    return (uxBits & WIFI_CONNECTED_BIT) != 0;
}
void wifi_set_disconnected(void){
    if (wifi_event_group != NULL) {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT); 
    }
}
void wifi_set_connected(void){
    if (wifi_event_group != NULL) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT); 
    }
}

void wifi_retry_task(void *pvParameters)
{
    int32_t receieved_event;

    while(true)
    {
        if(xQueueReceive(s_wifi_retry_event_queue, &receieved_event, portMAX_DELAY) == pdPASS)
        {
            if(receieved_event == WIFI_EVENT_STA_DISCONNECTED)
            {   
                wifi_set_disconnected();
                vTaskDelay(pdMS_TO_TICKS(retry_wait_time_ms)); 
                esp_wifi_connect();
                s_retries_count++;
                ESP_LOGI(TAG, "Connection interrupted. Attempting to reconnect #%d...", s_retries_count);
            }
        }
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "Disconnect reason: %d", event->reason);
        xQueueSend(s_wifi_retry_event_queue, &event_id, 0); 
    }
    
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Received IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retries_count = 0;
        wifi_set_connected();
    }
}

void wifi_init_sta(void)
{
    
    wifi_event_group = xEventGroupCreate();
    
    s_wifi_retry_event_queue = xQueueCreate(1, sizeof(int32_t));
    if (s_wifi_retry_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
    }
    
    xTaskCreate(&wifi_retry_task, "wifi_retry_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default()); 
    
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    
    esp_event_handler_instance_t instance_any_id; 
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id)); 
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    
    wifi_config_t wifi_config =
    {
        .sta =
        {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, 
        },
    };
    
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); 
    ESP_ERROR_CHECK(esp_wifi_start());

    
    ESP_LOGI(TAG, "WiFi STATION initialization finished");
    ESP_LOGI(TAG, "Connecting to SSID: %s", WIFI_SSID);
    
    
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY); 
    if (bits & WIFI_CONNECTED_BIT) ESP_LOGI(TAG, "Connected to SSID: %s", WIFI_SSID);
}