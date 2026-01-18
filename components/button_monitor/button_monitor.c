#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "arming_manager.h"
#include "button_monitor.h"

#define BOOT_BUTTON_PIN 0

// Time Thresholds (in Milliseconds)
#define SHORT_HOLD_MIN_MS  1000  // 1 second
#define SHORT_HOLD_MAX_MS  3000  // 3 seconds
#define ALARM_EXIT_HOLD_MS 3000  // 3 seconds to silence alarm
#define CONFIG_ENTER_MS    7000  // 7 seconds to enter config mode
#define RESET_HOLD_MS      15000 // 15 seconds for factory reset

static const char *TAG_BTN = "BUTTON_MONITOR";
#define NVS_NAMESPACE "storage"
#define KEY_FORCE_CONFIG "force_conf"

static EventGroupHandle_t button_event_group; 
#define ESP_RESTARTING_BIT (1UL << 0)

bool esp_is_restarting(void){
    if (button_event_group == NULL) return false;
    EventBits_t uxBits = xEventGroupGetBits(button_event_group);
    return (uxBits & ESP_RESTARTING_BIT) != 0;
}

void esp_set_restarting(void){
    if (button_event_group != NULL) {
        xEventGroupSetBits(button_event_group, ESP_RESTARTING_BIT); 
    }
}

// Helper to set config flag
void trigger_config_mode_reboot(void) {
    nvs_handle_t handle;
    ESP_LOGW(TAG_BTN, ">>> CONFIG MODE REQUESTED <<<");
    esp_set_restarting();
    
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, KEY_FORCE_CONFIG, 1);
        nvs_commit(handle);
        nvs_close(handle);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

void button_monitor_task(void *pvParameter)
{
    if (button_event_group == NULL) {
        button_event_group = xEventGroupCreate();
    }

    // Configure GPIO 0 (Boot Button)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, 
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    TickType_t press_start_time = 0;
    bool is_pressing = false;
    bool action_executed = false; // Prevents repeating action during a single press

    while (1) {
        // Active LOW button
        int level = gpio_get_level(BOOT_BUTTON_PIN);

        if (level == 0) { 
            // Button is pressed
            if (!is_pressing) {
                // Pressed
                press_start_time = xTaskGetTickCount();
                is_pressing = true;
                action_executed = false;
                ESP_LOGD(TAG_BTN, "Button Down");
            } 
            else {
                // Held
                TickType_t duration = xTaskGetTickCount() - press_start_time;
                uint32_t duration_ms = pdTICKS_TO_MS(duration);

                // Check for alarm silencing
                if (is_system_in_alarm()) {
                    if (!action_executed && duration_ms >= ALARM_EXIT_HOLD_MS) {
                        ESP_LOGW(TAG_BTN, ">>> SILENCING ALARM <<<");
                        clear_system_alarm(); // Disarms and clears alarm
                        action_executed = true; 
                    }
                }
                // (If disarmed) Check for Config Mode OR Factory Reset
                else if (!is_system_armed()) {
                    
                    // Priority 1: Factory Reset (Very Long Hold)
                    if (!action_executed && duration_ms >= RESET_HOLD_MS) {
                        ESP_LOGE(TAG_BTN, ">>> FACTORY RESET TRIGGERED <<<");
                        esp_set_restarting();
                        nvs_flash_erase(); 
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        esp_restart();
                        action_executed = true;
                    }
                    // Priority 2: Config Mode (Long Hold)
                    else if (!action_executed && duration_ms >= CONFIG_ENTER_MS) {
                        trigger_config_mode_reboot();
                        action_executed = true;
                    }
                }
            }
        } 
        else { 
            // Button releases
            if (is_pressing) {
                TickType_t duration = xTaskGetTickCount() - press_start_time;
                uint32_t duration_ms = pdTICKS_TO_MS(duration);
                is_pressing = false;

                if (!action_executed) {
                    // Only process release actions if a hold action didn't already happen
                    
                    if (is_system_in_alarm()) {
                        ESP_LOGW(TAG_BTN, "Ignored click in ALARM mode. Hold 3s to silence.");
                    }
                    else {
                        // Check for Arm/Disarm toggle
                        if (duration_ms >= SHORT_HOLD_MIN_MS && duration_ms < SHORT_HOLD_MAX_MS) {
                            toggle_arming_state();
                        }
                        else if (duration_ms < SHORT_HOLD_MIN_MS) {
                            ESP_LOGI(TAG_BTN, "Click too short (%lu ms). Ignored.", duration_ms);
                        }
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}