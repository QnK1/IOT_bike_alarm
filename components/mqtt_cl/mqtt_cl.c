#include "mqtt_client.h"
#include "esp_log.h"

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
            esp_mqtt_client_subscribe(client, "test/topic", 1);
            esp_mqtt_client_publish(client, "test/topic", "Hello from ESP32!", 0, 1, 0);
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

void mqtt_app_start(void)
{
    ESP_LOGI(TAG, "Initializing MQTT client...");
    // Hardcoded for demonstration purposes, will probably differ in your case
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://10.227.164.210:1883",
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
}
