    idf_component_register(SRCS "lora.c"
                        INCLUDE_DIRS "include"
                        REQUIRES driver config nvs_store mpu6050 arming_manager
                        PRIV_REQUIRES)