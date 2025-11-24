idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS ""
                       REQUIRES esp_wifi nvs_flash driver wifi http bt mqtt_cl esp_timer
                       )

                       