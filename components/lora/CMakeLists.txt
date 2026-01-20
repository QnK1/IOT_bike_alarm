    idf_component_register(SRCS "lora.c"
                        INCLUDE_DIRS "include"
                        REQUIRES driver config nvs_store
                        PRIV_REQUIRES arming_manager)