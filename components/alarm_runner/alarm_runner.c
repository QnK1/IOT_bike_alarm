#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "arming_manager.h"
#include "gps.h"
#include "mqtt_cl.h"
#include "mqtt_client.h"
#include "lora.h"
#include "nvs_store.h"

static const char *TAG = "ALARM_RUNNER";

void alarm_runner_task(void *pvParameter)
{
    bool gps_active = false;

    while (1) {
        if (is_system_in_alarm()) {
            
            // 1. Wake GPS if not already active
            if (!gps_active) {
                ESP_LOGW(TAG, "Alarm triggered! Waking GPS...");
                gps_wake();
                gps_active = true;
                // Give GPS module time to wake up and start acquiring satellites
                vTaskDelay(pdMS_TO_TICKS(500)); 
            }

            // 2. Get Real Coordinates
            gps_data_t coords = gps_get_coordinates();

            if (coords.is_valid) {
                // Log valid coordinates (This is where you would eventually send HTTP POST)
                ESP_LOGE(TAG, "ALARM ACTIVE: Valid Fix! Lat: %.5f, Lon: %.5f, Sats: %d",
                         coords.latitude, 
                         coords.longitude,
                         coords.satellites);
                    
                    // if (mqtt_is_connected()) {
                    //     char payload[128];
                    //     snprintf(payload, sizeof(payload),
                    //         "{\"lat\":%.6f,\"lon\":%.6f,\"sats\":%d}",
                    //         coords.latitude,
                    //         coords.longitude,
                    //         coords.satellites
                    //     );

                    //     esp_mqtt_client_publish(
                    //         mqtt_get_client(),
                    //         "system_iot/user_001/esp32/gps",
                    //         payload,
                    //         0,
                    //         1,
                    //         0
                    //     );

                    //     ESP_LOGI(TAG, "MQTT Sent: %s", payload);
                    // }
                    char user[64];
                    char device[64];
                    nvs_load_user_id(user, 64);
                    nvs_load_device_id(device, 64);
                    char payload[128];
                    snprintf(payload, sizeof(payload),
                        "{\"lat\":%.6f,\"lon\":%.6f,\"sats\":%d}",
                        coords.latitude,
                        coords.longitude,
                        coords.satellites
                    );
                    char message[256];
                    int len = snprintf(message, sizeof(message), 
                        "<system_iot/%s/%s/gps=%s>", user, device, payload);
                    lora_send((uint8_t*)message, len);
                    ESP_LOGI(TAG, "LORA Sent: %s", payload);

            } else {
                // char payload[64];
                // snprintf(payload, sizeof(payload),
                //         "{\"gps_fix\":false,\"sats\":%d}", coords.satellites);

                // esp_mqtt_client_publish(
                //     mqtt_get_client(),
                //     "system_iot/user_001/esp32/gps/status",
                //     payload,
                //     0,
                //     0,
                //     0
                // );

                char user[64];
                char device[64];
                nvs_load_user_id(user, 64);
                nvs_load_device_id(device, 64);
                char payload[128];
                snprintf(payload, sizeof(payload),
                        "{\"gps_fix\":false,\"sats\":%d}", coords.satellites);
                char message[256];
                int len = snprintf(message, sizeof(message), 
                    "<system_iot/%s/%s/gps/status=%s>", user, device, payload);
                lora_send((uint8_t*)message, len);
                ESP_LOGW(TAG, "LORA Status Sent: %s", payload);
            }

            // 1 second delay between logs/updates
            vTaskDelay(pdMS_TO_TICKS(1000));

        } else {
            // System is NOT in alarm
            
            // 3. Sleep GPS if it was active
            if (gps_active) {
                ESP_LOGI(TAG, "Alarm cleared. Putting GPS to sleep.");
                gps_sleep();
                gps_active = false;
            }

            // Idle wait
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}