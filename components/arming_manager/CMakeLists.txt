idf_component_register(SRCS "arming_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver wifi button_monitor mqtt_cl lora nvs_store config)