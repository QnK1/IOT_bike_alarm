#include "lora.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "LORA";
static SemaphoreHandle_t lora_uart_mutex = NULL;

// Pomocnicza funkcja czekająca, aż moduł skończy pracę
static void wait_for_aux() {
    // Czekaj tak długo, aż AUX będzie w stanie niskim (0 = gotowy)
    while (gpio_get_level(LORA_AUX_PIN)) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t lora_init(void) {
    if (lora_uart_mutex == NULL) {
        lora_uart_mutex = xSemaphoreCreateMutex();
    }
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
    ESP_LOGI(TAG, "Waiting for send");
    if (xSemaphoreTake(lora_uart_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        ESP_LOGI(TAG, "Waiting for send, aux");
        wait_for_aux(); 
        ESP_LOGI(TAG, "Sending");
        int sent = uart_write_bytes(LORA_UART_PORT, (const char*)data, len);
        // Opcjonalnie: poczekaj aż AUX wróci do High po wysłaniu
        // wait_for_aux(); 
        xSemaphoreGive(lora_uart_mutex);
        return sent;
    }
    ESP_LOGE("LORA", "Could not get UART Mutex for sending!");
    return -1;
}

int lora_receive(uint8_t* buffer, uint32_t size, uint32_t timeout_ms) {
    if (xSemaphoreTake(lora_uart_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        int len = uart_read_bytes(LORA_UART_PORT, buffer, size, pdMS_TO_TICKS(timeout_ms));
        xSemaphoreGive(lora_uart_mutex);
        return len;
    }
    return 0;
}