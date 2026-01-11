#include "mqtt_client.h"
#include "mqtt_cl.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"

static const char *TAG = "MQTT_CL";

// All that represents a working MQTT Client 
static esp_mqtt_client_handle_t client;

esp_mqtt_client_handle_t mqtt_get_client(void)
{
    return client;
}

static bool mqtt_connected = false;

bool mqtt_is_connected(void)
{
    return mqtt_connected;
}

// This function is called whenever an event occurs in MQTT
static void mqtt_event_handler(void *handler_args,    // Additional data 
                               esp_event_base_t base, // Type of an event
                               int32_t event_id,      // Specific event
                               void *event_data)      // Data of the event
{
    // Casting void pointer to a correct type
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id)
    {
        // Client successfully connected to broker
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            ESP_LOGI(TAG, "MQTT connected!");
            break;

        // Client could not connect to broker
        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            ESP_LOGW(TAG, "MQTT disconnected!");
            break;

        // Broker sent a message about a subscribed topic
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT DATA RECEIVED:");
            printf("TOPIC: %.*s\n", event->topic_len, event->topic);
            printf("DATA:  %.*s\n", event->data_len, event->data);
            break;

        // Logging all other events
        default:
            ESP_LOGI(TAG, "Other MQTT event id: %ld", event_id);
            break;
    }
}

void mqtt_app_start(void)
{
    ESP_LOGI(TAG, "Initializing MQTT client...");

    // Initialization of a configuration structure with broker's ip address, username and password
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://10.85.58.210:1883",
        .credentials.username = "myuser",
        .credentials.authentication.password = "1234",
    };

    // Initializing a new client with config struct
    client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(
        client,             // Client that the event handler is assigned to
        ESP_EVENT_ANY_ID,   // Function handles all possible events
        mqtt_event_handler, // Function to call when an event occurs
        NULL                // Additional data to be passed into function
    );

    esp_mqtt_client_start(client);
}
