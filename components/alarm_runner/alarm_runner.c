#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "arming_manager.h"
#include "gps.h" // <--- Added

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
            } else {
                // Log searching status
                ESP_LOGW(TAG, "ALARM ACTIVE: Searching for satellites... (Visible: %d)", coords.satellites);
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