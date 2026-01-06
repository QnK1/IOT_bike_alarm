idf_component_register(SRCS "mpu_monitor.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_timer mpu6050 arming_manager config)