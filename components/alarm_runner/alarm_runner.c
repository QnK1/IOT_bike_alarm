#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "arming_manager.h"

static const char *TAG = "ALARM_RUNNER";

void alarm_runner_task(void *pvParameter)
{
    while (1) {
        if (is_system_in_alarm()) {
            // ALARM IS ACTIVE
            //TEMPORARY, TO BE REPLACED WITH ACTUAL GPS DATA
            ESP_LOGE(TAG, "ALARM ACTIVE: Sending GPS Coordinates... [50.2649° N, 19.0238° E]");
            // Mocking network delay
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            // Idle when no alarm
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}