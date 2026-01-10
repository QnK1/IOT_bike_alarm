#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

void battery_init(void);
uint32_t battery_get_voltage_mv(void);
uint8_t battery_get_percentage(void);

// Task to log battery level periodically
void battery_monitor_task(void *pvParameter);

#endif