/*
 * gps.h
 * Interface for GY-GPS6MU2 / NEO-6M GPS Module
 */

#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <stdint.h>


// Data structure to hold parsed GPS information
typedef struct {
    float latitude;      // Decimal degrees (e.g., 48.8588)
    float longitude;     // Decimal degrees (e.g., 2.2943)
    bool is_valid;       // True only if GPS has a valid fix
    uint8_t satellites;  // Number of satellites currently tracked
} gps_data_t;

/**
 * @brief Initialize UART and start the background parsing task.
 * * @note Configures UART2 on GPIO 17 (TX) and GPIO 16 (RX) by default.
 */
void gps_init(void);

/**
 * @brief Retrieve the latest valid coordinates in a thread-safe manner.
 * * @return gps_data_t Copy of the latest data.
 */
gps_data_t gps_get_coordinates(void);

/**
 * @brief Send UBX command to put the module into low-power Backup Mode.
 * * @note Current consumption drops to ~500uA. 
 */
void gps_sleep(void);

/**
 * @brief Wake the module from sleep by generating UART activity.
 */
void gps_wake(void);

#endif // GPS_H