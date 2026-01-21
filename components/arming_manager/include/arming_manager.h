#ifndef ARMING_MANAGER_H
#define ARMING_MANAGER_H

#include <stdbool.h>

// Initialize
void arming_init(void);

// Getters
bool is_system_armed(void);
bool is_system_in_alarm(void);

// Setters
void set_system_armed(bool armed);
void toggle_arming_state(void);   // <--- Added this back
void trigger_system_alarm(void);  
void clear_system_alarm(void);    
void arming_lora_sender_task(void *pv);

#endif