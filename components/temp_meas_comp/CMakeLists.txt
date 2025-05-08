idf_component_register(SRCS "src/temp_meas_comp.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_adc esp_timer sensor_types
                    )