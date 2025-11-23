#include "mqtt_client.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"

static const char *TAG = "MQTT_CL";

static esp_mqtt_client_handle_t client;


static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected!");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected!");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT DATA RECEIVED:");
            printf("TOPIC: %.*s\n", event->topic_len, event->topic);
            printf("DATA:  %.*s\n", event->data_len, event->data);
            break;

        default:
            ESP_LOGI(TAG, "Other MQTT event id: %ld", event_id);
            break;
    }
}

// Musi być zdefiniowane globalnie, jak już masz w kodzie
// static esp_mqtt_client_handle_t client; 

void publisher_task(void *pvParameters)
{
    char topic[] = "system_iot/user_001/esp32_real/temperature";
    char payload[10];
    
    // Cyclically sending data
    while (1) {
        // 1. Check if client is initializrd
        if (client != NULL) {
            // 2. Random data generation
            float temp = 20.0 + ((float)(esp_random() % 100) / 10.0);
            sprintf(payload, "%.2f", temp);
            
            // 3. Sending data
            int msg_id = esp_mqtt_client_publish(
                client,  // Client
                topic,   // Topic
                payload, // Payload
                0,       // Length
                1,       // QoS
                0        // Retain
            );
            ESP_LOGI(TAG, "Sent: %s -> %s, msg_id=%d", topic, payload, msg_id);
        } else {
            ESP_LOGW(TAG, "MQTT Client is not ready.");
        }

        // Wait 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

    void mqtt_app_start(void)
{
    ESP_LOGI(TAG, "Initializing MQTT client...");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://10.85.58.210:1883",
        .credentials.username = "myuser",
        .credentials.authentication.password = "1234",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(
        client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );

    esp_mqtt_client_start(client);
    xTaskCreate(
        publisher_task, // Task Function Name
        "MQTT_PUBLISHER", // Task Name
        4096, // Stack Size (bytes)
        NULL, // Parameters passed to the task
        5, // Priority
        NULL // Task handle
    );
}
