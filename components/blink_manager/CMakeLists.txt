idf_component_register(SRCS "blink_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver wifi button_monitor ble_config)