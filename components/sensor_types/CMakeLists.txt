idf_component_register(SRCS "src/sensor_types.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_adc esp_timer time_manager
                    )