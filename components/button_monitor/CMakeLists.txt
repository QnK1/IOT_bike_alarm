idf_component_register(SRCS "button_monitor.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver nvs_flash arming_manager)