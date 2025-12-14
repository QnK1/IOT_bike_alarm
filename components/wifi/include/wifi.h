#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_rom_sys.h"

#ifndef WIFI_H
#define WIFI_H

void wifi_init_sta(char* wifi_ssid, char* wifi_pass);
bool wifi_is_connected(void);
void blink_task(void *pvParameter);

#endif