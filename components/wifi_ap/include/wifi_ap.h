#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <stdbool.h>
#include "esp_err.h"

// Returns true if AP mode is currently running (Mainly for Blink Logic)
bool wifi_is_ap_active(void);

// Starts the AP and Webserver
void wifi_start_ap_mode(void);

// Helpers used by Main
esp_err_t load_wifi_credentials(char* ssid, char* pass);
esp_err_t save_wifi_credentials(const char* ssid, const char* pass);

#endif