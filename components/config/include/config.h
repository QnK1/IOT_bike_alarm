#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

// --- GPS Pin Definitions ---
#define GPS_TXD_PIN (GPIO_NUM_26)  // ESP32 TX pin (Connects to GPS RX)
#define GPS_RXD_PIN (GPIO_NUM_27)  // ESP32 RX pin (Connects to GPS TX)

// --- UART Configuration ---
#define GPS_UART_PORT     (UART_NUM_2)
#define GPS_BAUD_RATE     (9600)
#define GPS_RX_BUF_SIZE   (1024)

// MPU config
#define MPU_SCL_IO (22)
#define MPU_SDA_IO (21)
#define MPU_DEVICE_ADDR (0x68)
#define MPU_I2C_PORT (0)

#endif // CONFIG_H