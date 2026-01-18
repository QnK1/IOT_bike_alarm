#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include "esp_err.h"

// Inicjalizacja sprzętu (UART + GPIO)
esp_err_t lora_init(void);

// Wysyłanie danych (z oczekiwaniem na gotowość modułu)
int lora_send(const uint8_t* data, uint32_t len);

// Odbieranie danych
int lora_receive(uint8_t* buffer, uint32_t size, uint32_t timeout_ms);

#endif