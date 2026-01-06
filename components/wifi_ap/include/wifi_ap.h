#ifndef WIFI_AP
#define WIFI_AP

#include "esp_err.h"

// Jeśli danych brak -> odpala AP + Serwer WWW
void wifi_ap_init(void);

// Funkcja pomocnicza do czyszczenia ustawień (np. pod przycisk BOOT)
// void wifi_ap_reset_settings(void);

bool wifi_is_ap_active(void);

#endif