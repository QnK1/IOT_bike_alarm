#ifndef MQTT_CL_H
#define MQTT_CL_H

#include "mqtt_client.h"

// Start MQTT client
void mqtt_app_start(void);

// Get MQTT client handle
esp_mqtt_client_handle_t mqtt_get_client(void);

// Check MQTT connection state
bool mqtt_is_connected(void);

#endif
