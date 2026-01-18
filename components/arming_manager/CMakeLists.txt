idf_component_register(SRCS "arming_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver wifi lora button_monitor mqtt_cl nvs_store)