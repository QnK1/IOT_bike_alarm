#include "mqtt_client.h"
#include "mqtt_cl.h"
#include "esp_log.h"
#include "arming_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "lora.h"
#include "esp_sntp.h"

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

void obtain_time(void) {
    ESP_LOGI("SNTP", "Obtaining time");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI("SNTP", "Czekam na czas systemowy... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI("SNTP", "Aktualny czas: %s", asctime(&timeinfo));
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
            
            esp_mqtt_client_subscribe(client, "system_iot/+/+/cmd", 1);
            ESP_LOGI(TAG, "Subscribed to cmd topic");
        break;

        // Client could not connect to broker
        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            ESP_LOGW(TAG, "MQTT disconnected!");
            break;

        // Broker sent a message about a subscribed topic
        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "MQTT DATA RECEIVED");

            char topic[event->topic_len + 1];
            char data[event->data_len + 1];

            memcpy(topic, event->topic, event->topic_len);
            topic[event->topic_len] = '\0';

            memcpy(data, event->data, event->data_len);
            data[event->data_len] = '\0';

            ESP_LOGI(TAG, "TOPIC: %s", topic);
            ESP_LOGI(TAG, "DATA: %s", data);

            char message[256];
            int len = snprintf(message, sizeof(message), "<%s=%s>", topic, data);

            // 4. Wysyłka przez radio
            if (len > 0) {
                lora_send((uint8_t*)message, len);
                ESP_LOGI(TAG, "LORA send: %s", message);
            }
                break;
            }


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
    extern const uint8_t ca_pem_start[]      asm("_binary_ca_pem_start");
    extern const uint8_t ca_pem_end[]        asm("_binary_ca_pem_end");

    extern const uint8_t client_crt_start[]  asm("_binary_client_crt_start");
    extern const uint8_t client_crt_end[]    asm("_binary_client_crt_end");

    extern const uint8_t private_key_start[] asm("_binary_private_key_start");
    extern const uint8_t private_key_end[]   asm("_binary_private_key_end");

    // W konfiguracji MQTT:
    // W konfiguracji MQTT (ESP-IDF v5.x):
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = "mqtts://a2y5v078fk91mi-ats.iot.eu-central-1.amazonaws.com:8883",
            .verification = {
                // Dla CA (Root Certificate) pole nazywa się 'data'
                .certificate = (const char *)ca_pem_start,
                .certificate_len = (size_t)(ca_pem_end - ca_pem_start),
            },
        },
        .credentials = {
            .client_id = "ESP_lora_to_mqtt",
            .authentication = {
                // Dla certyfikatu klienta pole nazywa się 'certificate' (bez _pem)
                .certificate = (const char *)client_crt_start,
                .certificate_len = (size_t)(client_crt_end - client_crt_start),
                
                // Dla klucza prywatnego pole nazywa się 'key' (bez _pem)
                .key = (const char *)private_key_start,
                .key_len = (size_t)(private_key_end - private_key_start),
            },
        }
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(
        client,             // Client that the event handler is assigned to
        ESP_EVENT_ANY_ID,   // Function handles all possible events
        mqtt_event_handler, // Function to call when an event occurs
        NULL                // Additional data to be passed into function
    );

    esp_mqtt_client_start(client);
    
}
