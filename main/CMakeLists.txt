idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS ""
                       REQUIRES esp_wifi nvs_flash driver wifi wifi_ap http bt mqtt_cl button_monitor blink_manager
                       )

                       