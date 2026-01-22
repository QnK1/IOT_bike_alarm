#include "gps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "config.h" 

#define GPS_TASK_STACK      4096

static const char *TAG = "GPS";

// Thread safety
static SemaphoreHandle_t gps_mutex = NULL;
static gps_data_t current_gps_data = {0};

// Internal parsing helpers
static void parse_nmea_line(char *line);
static float nmea_to_decimal(float nmea_coord, char quadrant);
static void send_ubx(uint8_t cls, uint8_t id, uint8_t *payload, uint16_t length);

#pragma pack(push, 1)

// UBX-RXM-PMREQ (Power Management Request)
typedef struct {
    uint32_t duration;  // 0 = Infinite
    uint32_t flags;     // 0x02 = Backup
} ubx_rxm_pmreq_t;

#pragma pack(pop)

static TaskHandle_t s_gps_task_handle = NULL;

void gps_sleep(void) {
    ESP_LOGI(TAG, "GPS: Sending Sleep Command...");
    if (s_gps_task_handle != NULL) vTaskSuspend(s_gps_task_handle);


    // 1. SEND SLEEP COMMAND (Backup Mode)
    // We ask the module to enter Backup Mode immediately.
    ubx_rxm_pmreq_t pmreq = {0};
    pmreq.duration = 0;     // Infinite sleep
    pmreq.flags = 0x02;     // Backup Mode (Force Sleep)
    send_ubx(0x02, 0x41, (uint8_t*)&pmreq, sizeof(pmreq));
    
    // 2. WAIT FOR TX COMPLETE
    uart_wait_tx_done(GPS_UART_PORT, 200); 
    
    // 3. FORCE TX HIGH
    uart_set_pin(GPS_UART_PORT, -1, -1, -1, -1);

    // Drive TX High (Idle state)
    gpio_config_t tx_conf = {
        .pin_bit_mask = (1ULL << GPS_TXD_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&tx_conf);
    gpio_set_level(GPS_TXD_PIN, 1); 

    // Set RX to Input with Pull-up
    gpio_config_t rx_conf = {
        .pin_bit_mask = (1ULL << GPS_RXD_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rx_conf);
    
    ESP_LOGI(TAG, "GPS: Sleep Sequence Complete (TX Held High).");
}

void gps_wake(void) {
    ESP_LOGI(TAG, "GPS: Waking Up...");

    // 1. RECONNECT UART PINS
    uart_set_pin(GPS_UART_PORT, GPS_TXD_PIN, GPS_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // 2. WAKE SEQUENCE (Force Baud Detection)
    uint8_t wake_bytes[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uart_write_bytes(GPS_UART_PORT, (const char*)wake_bytes, sizeof(wake_bytes));
    
    // 3. STABILIZATION DELAY
    // Give the GPS crystal time to stabilize and CPU to boot.
    vTaskDelay(pdMS_TO_TICKS(500));
    
    if (s_gps_task_handle != NULL) vTaskResume(s_gps_task_handle);
    ESP_LOGI(TAG, "GPS Awake.");
}

static void gps_task(void *pvParameters) {
    uint8_t *data = (uint8_t *) malloc(GPS_RX_BUF_SIZE);
    char line_buffer[128];
    int line_pos = 0;

    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate GPS buffer");
        vTaskDelete(NULL);
    }

    while (1) {
        // Read data from UART
        int len = uart_read_bytes(GPS_UART_PORT, data, GPS_RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);
        
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                char c = (char)data[i];
                if (c == '\n' || c == '\r') {
                    if (line_pos > 0) {
                        line_buffer[line_pos] = '\0';
                        if (line_buffer[0] == '$') {
                            parse_nmea_line(line_buffer);
                        }
                        line_pos = 0;
                    }
                } else {
                    if (line_pos < sizeof(line_buffer) - 1) {
                        line_buffer[line_pos++] = c;
                    } else {
                        // Buffer overflow protection: discard line
                        line_pos = 0;
                    }
                }
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
}

void gps_init(void) {
    gps_mutex = xSemaphoreCreateMutex();
    
    uart_config_t uart_config = {
        .baud_rate = GPS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(GPS_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_PORT, GPS_TXD_PIN, GPS_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_PORT, GPS_RX_BUF_SIZE * 2, 0, 0, NULL, 0));

    // Create the background parsing task
    xTaskCreate(gps_task, "gps_task", GPS_TASK_STACK, NULL, 4, &s_gps_task_handle);
    ESP_LOGI(TAG, "GPS Initialized.");

    // Allow time for GPS to output first messages before sleeping
    vTaskDelay(pdMS_TO_TICKS(1000));

    // STARTUP SLEEP: Put GPS to sleep immediately
    gps_sleep();
}

gps_data_t gps_get_coordinates(void) {
    gps_data_t result;
    if (xSemaphoreTake(gps_mutex, portMAX_DELAY) == pdTRUE) {
        result = current_gps_data;
        xSemaphoreGive(gps_mutex);
    } else {
        memset(&result, 0, sizeof(result));
    }
    return result;
}


static void parse_nmea_line(char *line) {
    if (strstr(line, "GGA") == NULL) return;

    char *token;
    char *rest = line;
    int field_index = 0;
    
    // Temporary storage for parsing
    float lat_raw = 0.0;
    float lon_raw = 0.0;
    char lat_dir = 0; 
    char lon_dir = 0;
    int fix_quality = 0;
    int sats = 0;

    // Standard NMEA splitting by comma
    while ((token = strtok_r(rest, ",", &rest))) {
        switch (field_index) {
            case 2: lat_raw = atof(token); break;
            case 3: lat_dir = token[0]; break;
            case 4: lon_raw = atof(token); break;
            case 5: lon_dir = token[0]; break;
            case 6: fix_quality = atoi(token); break;
            case 7: sats = atoi(token); break;
        }
        field_index++;
    }

    // Update global state safely
    if (xSemaphoreTake(gps_mutex, 100 / portTICK_PERIOD_MS) == pdTRUE) {
        current_gps_data.is_valid = (fix_quality > 0);
        current_gps_data.satellites = (uint8_t)sats;
        
        if (current_gps_data.is_valid) {
            current_gps_data.latitude = nmea_to_decimal(lat_raw, lat_dir);
            current_gps_data.longitude = nmea_to_decimal(lon_raw, lon_dir);
        }
        xSemaphoreGive(gps_mutex);
    }
}

static float nmea_to_decimal(float nmea_coord, char quadrant) {
    // DDMM.MMMM -> DD.DDDD
    int degrees = (int)(nmea_coord / 100);
    float minutes = nmea_coord - (degrees * 100);
    float decimal = degrees + (minutes / 60.0f);
    
    if (quadrant == 'S' || quadrant == 'W') {
        decimal *= -1.0f;
    }
    return decimal;
}

static void send_ubx(uint8_t cls, uint8_t id, uint8_t *payload, uint16_t length) {
    uint8_t head[6] = {0xB5, 0x62, cls, id, (uint8_t)(length & 0xFF), (uint8_t)(length >> 8)};
    uint8_t ck_a = 0, ck_b = 0;
    
    // Calculate checksum over Class, ID, Length, and Payload
    for (int i = 2; i < 6; i++) { ck_a += head[i]; ck_b += ck_a; }
    for (int i = 0; i < length; i++) { ck_a += payload[i]; ck_b += ck_a; }
    
    uart_write_bytes(GPS_UART_PORT, (const char*)head, 6);
    if (length > 0) {
        uart_write_bytes(GPS_UART_PORT, (const char*)payload, length);
    }
    
    uint8_t checksum[2] = {ck_a, ck_b};
    uart_write_bytes(GPS_UART_PORT, (const char*)checksum, 2);
}