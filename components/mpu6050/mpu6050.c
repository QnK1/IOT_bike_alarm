#include "mpu6050.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <math.h>

// --- Adresy MPU-6050 ---
#define MPU6050_ADDR                0x68    // Adres urządzenia (gdy AD0 = GND) 
#define REG_SMPLRT_DIV              0x19    // Dzielnik Częstotliwości Próbkowania (Sample Rate Divider)
#define REG_CONFIG                  0x1A    // Rejestr filtru DLPF
#define REG_GYRO_CONFIG             0x1B    // Rejestr czułości Żyroskopu
#define REG_ACCEL_CONFIG            0x1C    // Rejestr czułości Akcelerometru
#define REG_INT_ENABLE              0x38    // Rejestr włączania przerwań (np. Data Ready, Motion)
#define REG_INT_STATUS              0x3A    // Rejestr statusu przerwań
#define REG_ACCEL_XOUT_H            0x3B    // Pierwszy rejestr danych akcelerometru
#define REG_TEMP_OUT_H              0x41    // Pierwszy rejestr danych termometru
#define REG_GYRO_XOUT_H             0x43    // Pierwszy rejestr danych żyroskopu
#define REG_PWR_MGMT_1              0x6B    // Rejestr zasilania 1
#define REG_PWR_MGMT_2              0x6C    // Rejestr zasilania 2
#define REG_WHO_AM_I                0x75    // Rejestr ID Urządzenia

#define REG_MOT_THR                 0x1F    // Motion Detection Threshold
#define REG_MOT_DUR                 0x20    // Motion Detection Duration

#define I2C_MASTER_TX_BUF_DISABLE   0       // I2C master nie potrzebuje bufora
#define I2C_MASTER_RX_BUF_DISABLE   0

static const char *TAG = "MPU6050";

// Zmienne statyczne przechowujące aktualny dzielnik (skalę)
static float s_accel_scale = 16384.0f;
static float s_gyro_scale = 131.0f;

static mpu6050_offsets_t s_offsets = {0, 0, 0, 0, 0, 0};

static esp_err_t mpu6050_write_byte(uint8_t reg_addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Start komunikacji
    i2c_master_start(cmd);
    // Adres urządzenia + bit zapisu (0)
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    // Adres rejestru, do którego chcemy pisać
    i2c_master_write_byte(cmd, reg_addr, true);
    // Dane do zapisania
    i2c_master_write_byte(cmd, data, true);
    // Stop komunikacji
    i2c_master_stop(cmd);

    // Wykonanie komendy
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

static esp_err_t mpu6050_read_bytes(uint8_t reg_addr, uint8_t *data, size_t len) {
    if (len == 0) return ESP_OK;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // KROK 1: Wskazanie rejestru, od którego chcemy czytać
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);

    // KROK 2: Restart i faktyczny odczyt
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);

    // Czytamy len-1 bajtów z ACK, ostatni bajt z NACK (koniec transmisji)
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }

    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t mpu6050_init(void) {
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // Wewnętrzne pull-upy
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };

    esp_err_t err = i2c_param_config(i2c_master_port, &conf);
    if(err != ESP_OK) return err;

    err = i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if(err != ESP_OK) return err;

    ESP_LOGI(TAG, "I2C zainicjowane. Wybudzanie MPU...");

    return mpu6050_write_byte(REG_PWR_MGMT_1, 0);
}

// TEST POŁĄCZENIA
bool mpu6050_test_connection(void) {
    uint8_t device_id;
    if(mpu6050_read_bytes(REG_WHO_AM_I, &device_id, 1) == ESP_OK) {
        if (device_id == 0x68) return true;
    }
    return false;
}

esp_err_t mpu6050_set_accel_range(mpu6050_accel_range_t range) {
    switch (range) {
        case ACCEL_RANGE_2G:  s_accel_scale = 16384.0f; break;
        case ACCEL_RANGE_4G:  s_accel_scale = 8192.0f; break;
        case ACCEL_RANGE_8G:  s_accel_scale = 4096.0f; break;
        case ACCEL_RANGE_16G: s_accel_scale = 2048.0f; break;
        default: return ESP_ERR_INVALID_ARG;
    }

    // Zapisujemy bity do rejestru (bit 3 i 4)
    return mpu6050_write_byte(REG_ACCEL_CONFIG, range << 3);
}

esp_err_t mpu6050_set_gyro_range(mpu6050_gyro_range_t range) {
    switch (range) {
        case GYRO_RANGE_250DPS:  s_gyro_scale = 131.0f; break;
        case GYRO_RANGE_500DPS:  s_gyro_scale = 65.5f; break;
        case GYRO_RANGE_1000DPS: s_gyro_scale = 32.8f; break;
        case GYRO_RANGE_2000DPS: s_gyro_scale = 16.4f; break;
        default: return ESP_ERR_INVALID_ARG;
    }

    // Zapisujemy bity do rejestru (bit 3 i 4)
    return mpu6050_write_byte(REG_GYRO_CONFIG, range << 3);
}

esp_err_t mpu6050_set_dlpf_mode(mpu6050_dlpf_t mode) {
    return mpu6050_write_byte(REG_CONFIG, mode);
}

esp_err_t mpu6050_set_sample_rate_divider(uint8_t divider) {
    return mpu6050_write_byte(REG_SMPLRT_DIV, divider);
}

esp_err_t mpu6050_enable_data_ready_interrupt(void) {
    return mpu6050_write_byte(REG_INT_ENABLE, 0x01);
}

esp_err_t mpu6050_get_acceleration(mpu6050_acceleration_t *accel) {
    uint8_t raw_data[6];
    // Czytaj 6 bajtów zaczynając od ACCEL_XOUT_H
    if (mpu6050_read_bytes(REG_ACCEL_XOUT_H, raw_data, 6) == ESP_OK) {

        // Łączenie bajtów (High << 8 | Low)
        int16_t rax = (int16_t)((raw_data[0] << 8) | raw_data[1]);
        int16_t ray = (int16_t)((raw_data[2] << 8) | raw_data[3]);
        int16_t raz = (int16_t)((raw_data[4] << 8) | raw_data[5]);

        accel->x = (rax - s_offsets.accel_x) / s_accel_scale;
        accel->y = (ray - s_offsets.accel_y) / s_accel_scale;
        accel->z = (raz - s_offsets.accel_z) / s_accel_scale;

        return ESP_OK;
    }
    
    return ESP_FAIL;
}

esp_err_t mpu6050_get_rotation(mpu6050_rotation_t *gyro) {
    uint8_t raw_data[6];
    // Czytaj 6 bajtów zaczynając od GYRO_XOUT_H
    if (mpu6050_read_bytes(REG_GYRO_XOUT_H, raw_data, 6) == ESP_OK) {

        // Łączenie bajtów (High << 8 | Low)
        int16_t rgx = (int16_t)((raw_data[0] << 8) | raw_data[1]);
        int16_t rgy = (int16_t)((raw_data[2] << 8) | raw_data[3]);
        int16_t rgz = (int16_t)((raw_data[4] << 8) | raw_data[5]);

        gyro->x = (rgx - s_offsets.gyro_x) / s_gyro_scale;
        gyro->y = (rgy - s_offsets.gyro_y) / s_gyro_scale;
        gyro->z = (rgz - s_offsets.gyro_z) / s_gyro_scale;

        return ESP_OK;
    }
    
    return ESP_FAIL;
}

esp_err_t mpu6050_get_temperature(float *temp) {
    uint8_t raw_data[2];
    if (mpu6050_read_bytes(REG_TEMP_OUT_H, raw_data, 2) == ESP_OK) {
        int16_t rtemp = (uint16_t)((raw_data[0] << 8) | raw_data[1]);
        *temp = (rtemp / 340.0f) + 36.53f;
        
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t mpu6050_get_data(mpu6050_data_t *data) {
    uint8_t raw_data[14];
    // Czytaj 14 bajtów zaczynając od ACCEL_XOUT_H (0x3B)
    // Kolejność w pamięci MPU: Accel(6) -> Temp(2) -> Gyro(6)
    if (mpu6050_read_bytes(REG_ACCEL_XOUT_H, raw_data, 14) == ESP_OK) {

        // Łączenie bajtów (High << 8 | Low)
        int16_t rax = (int16_t)((raw_data[0] << 8) | raw_data[1]);
        int16_t ray = (int16_t)((raw_data[2] << 8) | raw_data[3]);
        int16_t raz = (int16_t)((raw_data[4] << 8) | raw_data[5]);

        int16_t rtemp = (int16_t)((raw_data[6] << 8) | raw_data[7]);

        int16_t rgx = (int16_t)((raw_data[8] << 8) | raw_data[9]);
        int16_t rgy = (int16_t)((raw_data[10] << 8) | raw_data[11]);
        int16_t rgz = (int16_t)((raw_data[12] << 8) | raw_data[13]);

        // Konwersja na jednostki czytelne dla człowieka
        // Domyślna czułość Accel to +/- 2g (dzielnik 16384.0)
        // Domyslna czułość Gyro to +/- 250deg/s (dzielnik 131.0)

        data->ax = (rax - s_offsets.accel_x) / s_accel_scale;
        data->ay = (ray - s_offsets.accel_y) / s_accel_scale;
        data->az = (raz - s_offsets.accel_z) / s_accel_scale;

        data->gx = (rgx - s_offsets.gyro_x) / s_gyro_scale;
        data->gy = (rgy - s_offsets.gyro_y) / s_gyro_scale;
        data->gz = (rgz - s_offsets.gyro_z) / s_gyro_scale;

        data->temp = (rtemp / 340.0f) + 36.53f;

        return ESP_OK;
    }
    
    return ESP_FAIL;
}

esp_err_t mpu6050_calibrate(uint16_t iterations) {
    ESP_LOGI(TAG, "Rozpoczynam kalibrację... Nie ruszaj czujnikiem!");

    int32_t ax_sum = 0, ay_sum = 0, az_sum = 0;
    int32_t gx_sum = 0, gy_sum = 0, gz_sum = 0;
    uint8_t raw_data[14];

    for (int i = 0; i < iterations; i++) {
        if(mpu6050_read_bytes(REG_ACCEL_XOUT_H, raw_data, 14) != ESP_OK) {
            return ESP_FAIL;
        }

        ax_sum += (int16_t)((raw_data[0] << 8) | raw_data[1]);
        ay_sum += (int16_t)((raw_data[2] << 8) | raw_data[3]);
        az_sum += (int16_t)((raw_data[4] << 8) | raw_data[5]);

        gx_sum += (int16_t)((raw_data[8] << 8) | raw_data[9]);
        gy_sum += (int16_t)((raw_data[10] << 8) | raw_data[11]);
        gz_sum += (int16_t)((raw_data[12] << 8) | raw_data[13]);

        vTaskDelay(pdMS_TO_TICKS(2));
    }

    s_offsets.accel_x = ax_sum / iterations;
    s_offsets.accel_y = ay_sum / iterations;
    s_offsets.accel_z = (az_sum / iterations) - (int16_t)s_accel_scale;

    s_offsets.gyro_x = gx_sum / iterations;
    s_offsets.gyro_y = gy_sum / iterations;
    s_offsets.gyro_z = gz_sum / iterations;

    ESP_LOGI(TAG, "KALIBRACJA ZAKOŃCZONA. Offsety: A[%d %d %d] G[%d %d %d]",
             s_offsets.accel_x, s_offsets.accel_y, s_offsets.accel_z,
             s_offsets.gyro_x, s_offsets.gyro_y, s_offsets.gyro_z);

    return ESP_OK;
}

mpu6050_offsets_t mpu6050_get_offsets(void) {
    return s_offsets;
}

esp_err_t mpu6050_enable_motion_detection(uint8_t threshold, uint8_t duration) {
    esp_err_t ret;

    // KONFIGURACJA ZASILANIA (Uśpienie żyroskopu, aktywacja Low-Power Accel)
    // REG_PWR_MGMT_1 (0x6B)
    ret = mpu6050_write_byte(REG_PWR_MGMT_1, 0x09);
    ret = mpu6050_write_byte(REG_PWR_MGMT_2, 0x07);

    // USTAWIENIA DETEKCJI
    ret = mpu6050_write_byte(REG_MOT_THR, threshold);
    if(ret != ESP_OK) return ret;

    ret = mpu6050_write_byte(REG_MOT_DUR, duration);
    if(ret != ESP_OK) return ret;

    // WŁĄCZENIE PRZERWANIA
    ret = mpu6050_write_byte(REG_INT_ENABLE, 0x40);
    if(ret != ESP_OK) return ret;

    // AKTYWACJA TRYBU LOW-POWER
    ret = mpu6050_write_byte(REG_CONFIG, 0x01);

    return ret;
}

uint8_t mpu6050_get_int_status(void) {
    uint8_t status;
    mpu6050_read_bytes(REG_INT_STATUS, &status, 1);
    return status;
}

esp_err_t mpu6050_set_normal_mode(void) {
    // 1. Zresetowanie PWR_MGMT_1 (wybudzenie wszystkiego, użycie zegara PLL)
    esp_err_t ret = mpu6050_write_byte(REG_PWR_MGMT_1, 0x01);
    if (ret != ESP_OK) return ret;

    // 2. Zresetowanie PWR_MGMT_2 (wyjście z trybu Standby)
    return mpu6050_write_byte(REG_PWR_MGMT_2, 0x00);
}

uint8_t mpu6050_read_register(uint8_t reg_addr) {
    uint8_t data;
    if(mpu6050_read_bytes(reg_addr, &data, 1) == ESP_OK) {
        return data;
    }

    return 0xFF;
}