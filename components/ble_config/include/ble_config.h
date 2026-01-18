#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include <stdbool.h>

void ble_config_init(void);
void ble_config_deinit(void);
bool ble_config_is_active(void);

#endif