#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "wifi.h"
#include "blink_manager.h"
#include "button_monitor.h"
#include "arming_manager.h"
#include "mpu_monitor.h"
#include "alarm_runner.h"
#include "gps.h"
#include "battery.h"
#include "mqtt_cl.h"
#include "ble_config.h"
#include "lora.h"

static const char *TAG = "LORA_GATEWAY";

// Funkcja pomocnicza do przetwarzania odebranej ramki
void process_lora_frame(char *raw_data, int len) {
    // 1. Szukamy ograniczników < oraz >
    char *start = strchr(raw_data, '<');
    char *end = strrchr(raw_data, '>'); // szukamy od końca dla pewności

    if (start && end && start < end) {
        *end = '\0';            // Zamykamy string na znaku '>'
        char *content = start + 1; // Przeskakujemy znak '<'

        // 2. Rozdzielamy temat od danych na znaku '='
        char *separator = strchr(content, '=');
        if (separator) {
            *separator = '\0';
            char *topic = content;
            char *payload = separator + 1;

            ESP_LOGI(TAG, "Odebrano LoRa -> Topic: %s | Payload: %s", topic, payload);

            // 3. Wysyłamy do MQTT (zakładając, że masz funkcję mqtt_get_client())
            // Jeśli nie masz jeszcze gotowego MQTT, to tutaj wstaw funkcję publikującą
            // esp_mqtt_client_publish(mqtt_get_client(), topic, payload, 0, 1, 0);

            esp_mqtt_client_publish(
                mqtt_get_client(),
                topic,
                payload,
                0,
                1,
                0
            );




        } else {
            ESP_LOGW(TAG, "Błędny format: brak znaku '='");
        }
    } else {
        ESP_LOGW(TAG, "Niepełna ramka danych (brak < lub >)");
    }
}

void lora_receiver_task(void *pvParameters) {
    static char msg_accumulator[512]; // Tu zbieramy fragmenty
    static int current_pos = 0;
    uint8_t temp_buffer[128]; 

    ESP_LOGI(TAG, "Uruchamiam nasłuchiwanie LoRa...");

    while (1) {
        // 1. Czytamy to, co aktualnie przyszło
        int len = lora_receive(temp_buffer, sizeof(temp_buffer), 100);

        for (int i = 0; i < len; i++) {
            char c = temp_buffer[i];

            // 2. Jeśli znajdziemy start, resetujemy bufor (nowa wiadomość)
            if (c == '<') {
                current_pos = 0;
            }

            // 3. Dodajemy znak do akumulatora
            if (current_pos < sizeof(msg_accumulator) - 1) {
                msg_accumulator[current_pos++] = c;
            }

            // 4. Jeśli znajdziemy koniec, przetwarzamy całość
            if (c == '>') {
                msg_accumulator[current_pos] = '\0';
                process_lora_frame(msg_accumulator, current_pos);
                current_pos = 0; // Gotowe, czekamy na następną
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    // 1. Inicjalizacja NVS (wymagane dla WiFi/MQTT)
    nvs_flash_init(); 
    // 2. Inicjalizacja Twojego modułu LoRa
    if (lora_init() == ESP_OK) {
        ESP_LOGI(TAG, "LoRa zainicjalizowana pomyślnie");
    } else {
        ESP_LOGE(TAG, "Błąd inicjalizacji LoRa!");
        return;
    }
    // 3.  Inicjalizacja WiFi i MQTT
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_sta("endzju", "royale123");
    obtain_time();
    mqtt_app_start();

    // 4. Uruchomienie zadania odbiorczego
    xTaskCreate(lora_receiver_task, "lora_receiver_task", 4096, NULL, 5, NULL);
}