#include "arming_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "wifi.h" 
#include "mqtt_cl.h"
#include "mqtt_client.h"
#include "lora.h"
#include "nvs_store.h"

static const char *TAG = "ARMING";
static EventGroupHandle_t arming_event_group;

#define SYSTEM_ARMED_BIT (1UL << 0)
#define SYSTEM_ALARM_BIT (1UL << 1)
#define SEND_STATUS_BIT (1UL << 2)

void arming_init(void) {
    if (arming_event_group == NULL) {
        arming_event_group = xEventGroupCreate();
    }
    xEventGroupClearBits(arming_event_group, SYSTEM_ARMED_BIT | SYSTEM_ALARM_BIT);
    xTaskCreate(&lora_receiver_task, "lora_rec", 8192, NULL, 6, NULL);
    xTaskCreate(&arming_lora_sender_task, "arming_lora_send", 4096, NULL, 5, NULL);
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
        xEventGroupSetBits(arming_event_group, SYSTEM_ARMED_BIT);
        ESP_LOGW(TAG, ">>> SYSTEM ARMED <<<");
    } else {
        xEventGroupClearBits(arming_event_group, SYSTEM_ARMED_BIT | SYSTEM_ALARM_BIT);
        if (is_system_in_alarm()){
            clear_system_alarm();
        }
        ESP_LOGW(TAG, ">>> SYSTEM DISARMED <<<");
    }
    // Prosimy o wysłanie statusu przez LoRa
    xEventGroupSetBits(arming_event_group, SEND_STATUS_BIT);
}

void arming_lora_sender_task(void *pv) {
    while(1) {
        // Czekaj aż pojawi się bit prośby o wysyłkę
        while (arming_event_group == NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        xEventGroupWaitBits(arming_event_group, SEND_STATUS_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        
        vTaskDelay(pdMS_TO_TICKS(500)); // Bezpieczny odstęp

        char user[64], device[64], message[256];
        nvs_load_user_id(user, 64);
        nvs_load_device_id(device, 64);
        
        bool armed = is_system_armed();
        int len = snprintf(message, sizeof(message), 
                  "<system_iot/%s/%s/armed={\"state\":\"%s\"}>", 
                  user, device, armed ? "ARMED" : "DISARMED");

        ESP_LOGI(TAG, "Sending: %s", message);
        lora_send((uint8_t*)message, len);
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
        char user[64];
        char device[64];
        nvs_load_user_id(user, 64);
        nvs_load_device_id(device, 64);
        char message[256];
        int len = snprintf(message, sizeof(message), "<system_iot/%s/%s/alarm={\"state\":\"START\"}>", user, device);
        lora_send((uint8_t*)message, len);
        ESP_LOGI(TAG, "LORA: SYSTEM DISARMED sent");

        xEventGroupSetBits(arming_event_group, SYSTEM_ALARM_BIT);
    }
}

void clear_system_alarm(void) {
    set_system_armed(false);
        char user[64];
        char device[64];
        nvs_load_user_id(user, 64);
        nvs_load_device_id(device, 64);
        char message[256];
        int len = snprintf(message, sizeof(message), "<system_iot/%s/%s/alarm={\"state\":\"STOP\"}>", user, device);
        lora_send((uint8_t*)message, len);
        ESP_LOGI(TAG, "LORA: Alarm STOP sent");
}