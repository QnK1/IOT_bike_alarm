#include "lora.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "esp_log.h"
#include "nvs_store.h"
#include "arming_manager.h"
#include "mpu6050.h"

static const char *TAG = "LORA";

// Pomocnicza funkcja czekająca, aż moduł skończy pracę
static void wait_for_aux() {
    // Czekaj tak długo, aż AUX będzie w stanie niskim (0 = gotowy)
    while (gpio_get_level(LORA_AUX_PIN)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t lora_init(void) {
    // 1. Konfiguracja UART
    uart_config_t uart_config = {
        .baud_rate = LORA_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(LORA_UART_PORT, 1024 * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LORA_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(LORA_UART_PORT, LORA_TX_PIN, LORA_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 2. Konfiguracja pinów sterujących (M0, M1 - Wyjścia)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LORA_M0_PIN) | (1ULL << LORA_M1_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    // 3. Konfiguracja pinu AUX (Wejście)
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << LORA_AUX_PIN);
    io_conf.pull_up_en = 1; // Podciągnięcie, jeśli moduł ma wyjście open-drain
    gpio_config(&io_conf);

    // 4. Ustawienie Trybu 0 (Normalny: M0=0, M1=0)
    gpio_set_level(LORA_M0_PIN, 0);
    gpio_set_level(LORA_M1_PIN, 0);

    wait_for_aux(); // Czekaj aż moduł wystartuje
    return ESP_OK;
}

int lora_send(const uint8_t* data, uint32_t len) {
    wait_for_aux(); // Nie wysyłaj, jeśli moduł jest zajęty
    int sent = uart_write_bytes(LORA_UART_PORT, (const char*)data, len);
    return sent;
}

int lora_receive(uint8_t* buffer, uint32_t size, uint32_t timeout_ms) {
    return uart_read_bytes(LORA_UART_PORT, buffer, size, pdMS_TO_TICKS(timeout_ms));
}

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
            char *data = separator + 1;

            char username[64] = "a";
            char device[64] = {0};
            char command[64] = {0};
            char username_nvs[64] = "b";

            nvs_load_user_id(username_nvs, sizeof(username_nvs));
            if(sscanf(topic, "system_iot/%63[^/]/%63[^/]/%63[^/]", username, device, command) != 3){
                ESP_LOGW(TAG, "Błędny format: %s", topic);
                return;
            }
            if (strcmp(username, username_nvs) != 0){
                ESP_LOGI(TAG, "Received LORA for other user -> Topic: %s | Data: %s", topic, data);
            }   
            if (strcmp(command, "cmd") == 0) {
                ESP_LOGI(TAG, "Received LORA -> Topic: %s | Data: %s", topic, data);
                
                if (strcmp(data, "ARM") == 0) {
                    set_system_armed(true);
                }
                else if (strcmp(data, "DISARM") == 0) {
                    set_system_armed(false);
                }

            } if (strcmp(command, "threshold") == 0) {
                ESP_LOGI(TAG, "Received LORA -> Topic: %s | Data: %s", topic, data);
                int threshold = atoi(data);
                mpu6050_enable_motion_detection(threshold, 1);
            } else {
                ESP_LOGI(TAG, "Unknown CMD: %s", command);
            }
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