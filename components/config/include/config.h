#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

// --- GPS Pin Definitions ---
#define GPS_TXD_PIN (GPIO_NUM_26)
#define GPS_RXD_PIN (GPIO_NUM_27)
#define GPS_UART_PORT (UART_NUM_2)
#define GPS_BAUD_RATE (9600)
#define GPS_RX_BUF_SIZE (1024)

// --- MPU Config ---
#define MPU_SCL_IO (22)
#define MPU_SDA_IO (21)
#define MPU_DEVICE_ADDR (0x68)
#define MPU_I2C_PORT (0)

// --- Battery Config ---
// ADC1_CHANNEL_6 is GPIO 34 on most ESP32 boards
#define BAT_ADC_CHANNEL    ADC_CHANNEL_6 
#define BAT_ADC_UNIT       ADC_UNIT_1
#define BAT_CHECK_PERIOD_MS (60000) // Log every 60 seconds

// Voltage Divider Config
#define BAT_R1_OHMS (24000)
#define BAT_R2_OHMS (24000)
#define BAT_VOLTAGE_FACTOR ((float)(BAT_R1_OHMS + BAT_R2_OHMS) / (float)BAT_R2_OHMS)

// Battery Characteristics (3x AAA Alkaline)
#define BAT_MAX_VOLTAGE (4500) // 4.5V (1.5V per cell)
#define BAT_MIN_VOLTAGE (3300) // 3.3V (1.1V per cell - empty)

// --- Lora Config ---
#define LORA_UART_PORT     (UART_NUM_1)
#define LORA_TX_PIN (GPIO_NUM_17)
#define LORA_RX_PIN (GPIO_NUM_16)
#define LORA_M0_PIN (GPIO_NUM_18)  
#define LORA_M1_PIN (GPIO_NUM_19)
#define LORA_AUX_PIN (GPIO_NUM_5)
#define LORA_BAUD_RATE     (9600)

#endif // CONFIG_H