#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOOT_BUTTON_PIN 0
#define LONG_PRESS_TIME_MS 1000 // 1 sekunda

static const char *TAG_BTN = "BUTTON_MONITOR";

static EventGroupHandle_t button_event_group; 
#define ESP_RESTARTING_BIT (1UL << 0)

bool esp_is_restarting(void){
    if (button_event_group == NULL) {
        return false;
    }
    EventBits_t uxBits = xEventGroupGetBits(button_event_group);
    return (uxBits & ESP_RESTARTING_BIT) != 0;
}
void esp_set_restarting(void){
    if (button_event_group != NULL) {
        xEventGroupSetBits(button_event_group, ESP_RESTARTING_BIT); 
    }
}

void button_monitor_task(void *pvParameter)
{
    if (button_event_group == NULL) {
        button_event_group = xEventGroupCreate();
    }
    // Konfiguracja pinu GPIO 0
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, 
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    TickType_t press_start_time = 0;
    bool pressed_now = false;

    while (1) {
        // Czy przycisk jest wciśnięty (stan LOW)
        if (gpio_get_level(BOOT_BUTTON_PIN) == 0) {
            
            if (!pressed_now) {
                // Początek wciśnięcia
                press_start_time = xTaskGetTickCount();
                pressed_now = true;
                ESP_LOGI(TAG_BTN, "Przycisk wciśnięty. Odliczanie...");
            } else {
                // Przycisk trzymany - sprawdź czas
                TickType_t current_press_duration = xTaskGetTickCount() - press_start_time;
                
                if (pdTICKS_TO_MS(current_press_duration) >= LONG_PRESS_TIME_MS) {
                    
                    ESP_LOGW(TAG_BTN, "Wykryto długie przytrzymanie (1s). Czyszczenie NVS...");
                    
                    // Usuwamy całą partycję NVS
                    nvs_flash_erase(); 
                    
                    esp_set_restarting();
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    
                    ESP_LOGI(TAG_BTN, "NVS wyczyszczone. Restart...");
                    esp_restart();
                }
            }
        } else {
            // Przycisk zwolniony
            pressed_now = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Sprawdzaj stan przycisku co 100 ms
    }
}