idf_component_register(SRCS "pressure_meas_comp.c"
                    REQUIRES esp_adc esp_timer sensor_types
                    INCLUDE_DIRS "include")