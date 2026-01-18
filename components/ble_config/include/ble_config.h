#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include <stdbool.h>

// Initializes BLE, sets up the GATT server for User ID configuration
void ble_config_init(void);

// Checks if a User ID is currently saved in NVS
bool is_user_assigned(void);

#endif