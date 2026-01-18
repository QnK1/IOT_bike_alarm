idf_component_register(SRCS "ble_config.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver nvs_flash bt nvs_store)