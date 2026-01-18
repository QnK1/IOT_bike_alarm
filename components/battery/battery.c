#include "battery.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "mqtt_cl.h"

static const char *TAG = "BATTERY";

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool do_calibration = false;

void battery_init(void) {
    // 1. Init ADC Unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BAT_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // 2. Configure Channel 
    // using DB_12 (approx 11dB) to measure up to ~3.1V
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, 
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BAT_ADC_CHANNEL, &config));

    // 3. Calibration Setup
    ESP_LOGI(TAG, "Calibration Scheme: Line Fitting");
    
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = BAT_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    // Use create_scheme_line_fitting instead of curve_fitting
    if (adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle) == ESP_OK) {
        do_calibration = true;
    } else {
        ESP_LOGW(TAG, "Calibration skipped, using raw data");
    }
}

uint32_t battery_get_voltage_mv(void) {
    int adc_raw;
    int voltage_at_pin_mv = 0;

    // Read Raw ADC
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &adc_raw));

    // Convert to Voltage (mV)
    if (do_calibration) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage_at_pin_mv));
    } else {
        // Fallback approximate: (Raw / 4095) * 3300mV
        // Note: ESP32 ADC is non-linear without calibration, so this is rough
        voltage_at_pin_mv = adc_raw * 3300 / 4095;
    }

    // Scale up by divider factor
    // V_bat = V_pin * (R1+R2)/R2
    uint32_t battery_voltage = (uint32_t)(voltage_at_pin_mv * BAT_VOLTAGE_FACTOR);
    
    return battery_voltage;
}

uint8_t battery_get_percentage(void) {
    uint32_t mv = battery_get_voltage_mv();
    
    if (mv >= BAT_MAX_VOLTAGE) return 100;
    if (mv <= BAT_MIN_VOLTAGE) return 0;

    // Linear mapping
    return (uint8_t)((mv - BAT_MIN_VOLTAGE) * 100 / (BAT_MAX_VOLTAGE - BAT_MIN_VOLTAGE));
}

void battery_monitor_task(void *pvParameter) {
    while (1) {
        uint32_t mv = battery_get_voltage_mv();
        uint8_t pct = battery_get_percentage();

        ESP_LOGI(TAG, "Battery: %lu mV (%d%%)", mv, pct);

        // Warning for low battery
        if (pct < 10) {
            ESP_LOGW(TAG, "BATTERY LOW! Please replace.");
        }

        // Send data through MQTT
        if (mqtt_is_connected()) {
            char payload[128];
            snprintf(payload, sizeof(payload),
                "{\"voltage_mv\":%lu,\"percentage\":%d}",
                mv,
                pct
            );

            // int msg_id = esp_mqtt_client_publish(
            //     mqtt_get_client(),
            //     "system_iot/user_001/esp32/battery",
            //     payload,
            //     0,
            //     1,
            //     0
            // );

            char user[64];
            nvs_load_user_id(user, 64);
            char message[256];
            int len = snprintf(message, sizeof(message),
                "<system_iot/%s/esp32/battery=%s>", user, payload);
            lora_send((uint8_t*)message, len);
            ESP_LOGI(TAG, "Battery LORA sent: %s", payload);

        } else {
            ESP_LOGW(TAG, "MQTT not connected");
        }

        vTaskDelay(pdMS_TO_TICKS(BAT_CHECK_PERIOD_MS));
    }
}