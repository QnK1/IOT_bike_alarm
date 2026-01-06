#ifndef MPU6050_H
#define MPU6050_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// --- Konfiguracja I2C ---
#define I2C_MASTER_SCL_IO           22      // GPIO dla zegara (SCL)
#define I2C_MASTER_SDA_IO           21      // GPIO dla danych (SDA)
#define I2C_MASTER_NUM_0            0       // Numer portu I2C
#define I2C_MASTER_NUM_1            1       // Numer portu I2C
#define I2C_MASTER_FREQ_HZ          400000  // Częstotliwość 400kHz

typedef struct {
    int scl_io;           // Numer pinu SCL
    int sda_io;           // Numer pinu SDA
    uint8_t device_addr;  // Adres I2C (0x68 lub 0x69)
    int i2c_port;         // Numer portu I2C (0 lub 1)
} mpu6050_config_t;

// --- Struktury danych ---

typedef struct {
    float x;
    float y;
    float z;
} mpu6050_acceleration_t;

typedef struct {
    float x;
    float y;
    float z;
} mpu6050_rotation_t;

// --- Opcje konfiguracyjne (Enumy) ---

/** Zakresy pomiarowe Akcelerometru */
typedef enum {
    ACCEL_RANGE_2G  = 0, // +/- 2g (Największa precyzja)
    ACCEL_RANGE_4G  = 1, // +/- 4g
    ACCEL_RANGE_8G  = 2, // +/- 8g
    ACCEL_RANGE_16G = 3  // +/- 16g (Dla dużych przeciążeń)
} mpu6050_accel_range_t;

/** Zakresy pomiarowe Żyroskopu*/
typedef enum {
    GYRO_RANGE_250DPS  = 0, // +/- 250 deg/s (Wolne, precyzyjne obroty)
    GYRO_RANGE_500DPS  = 1, // +/- 500 deg/s 
    GYRO_RANGE_1000DPS = 2, // +/- 1000 deg/s 
    GYRO_RANGE_2000DPS = 3  // +/- 2000 deg/s (Bardzo szybkie obroty) 
} mpu6050_gyro_range_t;

/** Filtr Dolnoprzepustowy (wygładzanie danych) */
typedef enum {
    DLPF_260HZ = 0, // Prawie brak filtrowania (duży szum, szybka reakcja)
    DLPF_184HZ = 1, 
    DLPF_94HZ  = 2, 
    DLPF_44HZ  = 3, 
    DLPF_21HZ  = 4, 
    DLPF_10HZ  = 5, 
    DLPF_5HZ   = 6, // Bardzo mocne filtrowanie (bardzo gładkie dane, wolniejsza reakcja) 
} mpu6050_dlpf_t;

/** Struktura przechowująca pełny zestaw danych */
typedef struct {
    float ax;    // Akcelerometr X (g)
    float ay;    // Akcelerometr Y (g)
    float az;    // Akcelerometr Z (g)
    float gx;    // Żyroskop X (deg/s)
    float gy;    // Żyroskop Y (deg/s)
    float gz;    // Żyroskop Z (deg/s)
    float temp;  // Temperatura (C)
} mpu6050_data_t;

// --- Struktura offsetów ---
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} mpu6050_offsets_t;

// --- Funkcje sterownika ---

/**
 * @brief Inicjalizuje I2C i wybudza MPU-6050
 */
esp_err_t mpu6050_init(const mpu6050_config_t *conf);

/**
 * Sprawdza połączenie.
 * @return true jeśli czujnik odpowiada poprawnie (0x68), false jeśli błąd.
*/
bool mpu6050_test_connection(void);

// Funkcje konfiguracyjne
esp_err_t mpu6050_set_accel_range(mpu6050_accel_range_t range);
esp_err_t mpu6050_set_gyro_range(mpu6050_gyro_range_t range);
esp_err_t mpu6050_set_dlpf_mode(mpu6050_dlpf_t mode);

/**
 * Ustawia częstotliwość próbkowania.
 * rate = 1000 / (1 + divider)
 * Np. divider=9 daje 100Hz (jeśli DLPF włączony)
 */
esp_err_t mpu6050_set_sample_rate_divider(uint8_t divider);

/**
 * Włącza przerwanie "Data Ready" na pinie INT czujnika.
 * Przydatne, gdy chcemy czytać dane tylko wtedy, gdy są gotowe.
 *
 */
esp_err_t mpu6050_enable_data_ready_interrupt(void);

// --- Odczyt danych ---
esp_err_t mpu6050_get_acceleration(mpu6050_acceleration_t *accel);
esp_err_t mpu6050_get_rotation(mpu6050_rotation_t *gyro);
esp_err_t mpu6050_get_temperature(float *temp);


/**
 * @brief Odczytuje akcelerometr, żyroskop i temperaturę,
 * a następnie konwertuje je na wartości czytelne (g, deg/s, C).
 * * @param ax Wskaźnik do zmiennej, gdzie zapisać przyspieszenie X (g)
 * @param ay Wskaźnik do zmiennej, gdzie zapisać przyspieszenie Y (g)
 * @param az Wskaźnik do zmiennej, gdzie zapisać przyspieszenie Z (g)
 * @param temp Wskaźnik do zmiennej, gdzie zapisać temperaturę (C)
 */
esp_err_t mpu6050_get_data(mpu6050_data_t *data);

/**
 * @brief Wykonuje automatyczną kaibrację czujnika.
 * UWAGA: Podczas tej operacji czujnik musi leżeć płasko i nieruchomo.
 * Funkcja pobiera 'iterations' próbek, uśrednia szum i ustawia offsety.
 * * @param iterations Liczba próbek. Im więcej tym dokładniej, ale dłużej.
*/
esp_err_t mpu6050_calibrate(uint16_t iterations);

/**
 * @brief Konfiguruje sprzętowe wykrywanie ruchu (Motion Detection).
 * Czujnik wyśle sygnał przerwania, gdy wykryje ruch powyżej progu.
 * * @param threshold Próg siły ruchu (1-255). 1 = 2mg, 255 = 510mg.
 * * @param duration Czas trwania ruchu (1-255ms). Jak długo musi trwać ruch.
*/
esp_err_t mpu6050_enable_motion_detection(uint8_t threshold, uint8_t duration);

/**
 * @brief Pobiera aktualnie używane offsety.
*/
mpu6050_offsets_t mpu6050_get_offsets(void);

/**
 * @brief Odczytuje wartość rejestru statusu przerwań.
 * @return Wartość rejestru. Bit 6 (0x40) oznacza wykrycie ruchu.
 */
uint8_t mpu6050_get_int_status(void);

/**
 * @brief Przełącza MPU-6050 w tryb normalnej pracy (pełne zasilanie, bez uśpienia).
 * Wyłącza żyroskop, jeśli był ustawiony w trybie standby przez Low-Power.
 * @return ESP_OK w przypadku sukcesu.
 */
esp_err_t mpu6050_set_normal_mode(void);

/**
 * @brief Pomocnicza funkcja do odczytu pojedynczego rejestru.
 * @param reg_addr Adres rejestru do odczytu.
 * @return Wartość rejestru (8-bit)
 */
uint8_t mpu6050_read_register(uint8_t reg_addr);

#endif