idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS ""
                       REQUIRES esp_wifi nvs_flash driver wifi wifi_ap mqtt_cl button_monitor blink_manager arming_manager mpu_monitor alarm_runner
                       )

                       