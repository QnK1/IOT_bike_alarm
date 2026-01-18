idf_component_register(SRCS "nvs_store.c"
                    INCLUDE_DIRS "include"
                    REQUIRES wifi esp_wifi driver esp_http_server nvs_flash)