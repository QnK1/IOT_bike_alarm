idf_component_register(SRCS "alarm_runner.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver wifi wifi_ap button_monitor arming_manager)