#include "arming_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "wifi.h" 
#include "mqtt_cl.h"
#include "mqtt_client.h"

static const char *TAG = "ARMING";
static EventGroupHandle_t arming_event_group;

#define SYSTEM_ARMED_BIT (1UL << 0)
#define SYSTEM_ALARM_BIT (1UL << 1)

void arming_init(void) {
    if (arming_event_group == NULL) {
        arming_event_group = xEventGroupCreate();
    }
    xEventGroupClearBits(arming_event_group, SYSTEM_ARMED_BIT | SYSTEM_ALARM_BIT);
}

bool is_system_armed(void) {
    if (arming_event_group == NULL) return false;
    return (xEventGroupGetBits(arming_event_group) & SYSTEM_ARMED_BIT) != 0;
}

bool is_system_in_alarm(void) {
    if (arming_event_group == NULL) return false;
    return (xEventGroupGetBits(arming_event_group) & SYSTEM_ALARM_BIT) != 0;
}

void set_system_armed(bool armed) {
    if (arming_event_group == NULL) return;
    
    if (armed) {
        ESP_LOGW(TAG, ">>> SYSTEM ARMED <<<");
        xEventGroupSetBits(arming_event_group, SYSTEM_ARMED_BIT);
        xEventGroupClearBits(arming_event_group, SYSTEM_ALARM_BIT);

        if (mqtt_is_connected()) {
            esp_mqtt_client_publish(
                mqtt_get_client(),
                "system_iot/user_001/esp32/armed",
                "{\"state\":\"ARMED\"}",
                0,
                1,
                0
            );
            ESP_LOGI(TAG, "MQTT: SYSTEM ARMED sent");
        }

    } else {
        ESP_LOGW(TAG, ">>> SYSTEM DISARMED <<<");
        xEventGroupClearBits(arming_event_group, SYSTEM_ARMED_BIT | SYSTEM_ALARM_BIT);
        wifi_reset_retry_logic(); 

        if (mqtt_is_connected()) {
            esp_mqtt_client_publish(
                mqtt_get_client(),
                "system_iot/user_001/esp32/armed",
                "{\"state\":\"DISARMED\"}",
                0,
                1,
                0
            );
            ESP_LOGI(TAG, "MQTT: SYSTEM DISARMED sent");
        }
    }
}

void toggle_arming_state(void) {
    if (is_system_armed()) {
        set_system_armed(false);
    } else {
        set_system_armed(true);
    }
}

void trigger_system_alarm(void) {
    if (arming_event_group == NULL) return;
    if (is_system_armed()) {
        ESP_LOGE(TAG, "!!! ALARM TRIGGERED !!!");

        if (mqtt_is_connected()) {

            esp_mqtt_client_publish(
                mqtt_get_client(),
                "system_iot/user_001/esp32/alarm",
                "{\"state\":\"START\"}",
                0,
                1,
                0
            );

            ESP_LOGI(TAG, "MQTT Alarm START sent");
        }

        xEventGroupSetBits(arming_event_group, SYSTEM_ALARM_BIT);
    }
}

void clear_system_alarm(void) {
    set_system_armed(false);

        if (mqtt_is_connected()) {

        esp_mqtt_client_publish(
            mqtt_get_client(),
            "system_iot/user_001/esp32/alarm",
            "{\"state\":\"STOP\"}",
            0,
            1,
            0
        );

        ESP_LOGI(TAG, "MQTT Alarm STOP sent");
    }
}