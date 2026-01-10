#include "mqtt_client.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"

#include "gps.h"

static const char *TAG = "MQTT_CL";

// All that represents a working MQTT Client 
static esp_mqtt_client_handle_t client;

static bool mqtt_connected = false;

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

void mqtt_gps_publisher_task(void *pvParameters)
{
    char payload[128]; // Data to send (defined later)
    const char *topic = "system_iot/user_001/esp32/gps"; // Where to send data

    // Cyclically sending data
    while (true) {

        if (!mqtt_connected) {
            ESP_LOGW("MQTT_GPS", "MQTT not connected, waiting...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        gps_wake();

        gps_data_t gps;
        int timeout = 60;

        do {
            vTaskDelay(pdMS_TO_TICKS(1000));
            gps = gps_get_coordinates();
        } while (!gps.is_valid && --timeout > 0);

        if (!gps.is_valid) {
            ESP_LOGW("MQTT_GPS", "No GPS fix, skipping publish");
            gps_sleep();
            continue;
        }


        snprintf(payload, sizeof(payload),
            "{"
            "\"lat\":%.6f,"
            "\"lon\":%.6f,"
            "\"sats\":%d"
            "}",
            gps.latitude,
            gps.longitude,
            gps.satellites
        );

        esp_mqtt_client_publish(
            client,  // Client - handler declared at the beginning
            topic,   // Topic - where the data is sent to
            payload, // Payload - what data is sent
            0,       // Length - length of payload in bytes (if 0 then calculated automatically)
            1,       // QoS - quality of service (broker confirms that it received a message, in rare cases subscriber may receive it a few times)
            0        // Retain - retain flag for messages in broker (for 0, it won't retain this message and won't send it to new subscribers)
        );
        
        // Logs that message has been sent and prints out its details
        ESP_LOGI("MQTT_GPS", "GPS sent: %s", payload);
        gps_sleep();

        // Wait 5 minutes
        vTaskDelay(pdMS_TO_TICKS(300000));
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
    xTaskCreate(
        mqtt_gps_publisher_task,    // Task Function
        "MQTT_GPS",                 // Task Name
        4096,                       // Stack Size (bytes)
        NULL,                       // Parameters passed to the task - pointer to data passed to task function
        6,                          // Priority - the bigger the more important the task is (important ones are executed first).
        NULL                        // Task handle - can be used to refer to the task later in code
    );
}
