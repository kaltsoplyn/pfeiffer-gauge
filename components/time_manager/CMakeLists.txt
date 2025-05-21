idf_component_register(SRCS "src/time_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos esp_timer lwip)