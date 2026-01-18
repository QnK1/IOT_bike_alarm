idf_component_register(SRCS "alarm_runner.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver wifi button_monitor arming_manager gps mqtt_cl)