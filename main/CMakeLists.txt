idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS ""
                       REQUIRES esp_wifi nvs_flash driver wifi mqtt_cl lora button_monitor blink_manager arming_manager mpu_monitor alarm_runner gps battery ble_config nvs_store lora
                       )

                       