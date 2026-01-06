#ifndef BUTTON_MONITOR_H
#define BUTTON_MONITOR_H

void button_monitor_task(void *pvParameter);
bool esp_is_restarting(void);

#endif