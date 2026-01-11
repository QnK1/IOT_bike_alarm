idf_component_register(SRCS "arming_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver wifi wifi_ap button_monitor mqtt_cl)