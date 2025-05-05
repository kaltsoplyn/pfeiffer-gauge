idf_component_register(SRCS "pressure_meas_comp.c"
                    REQUIRES esp_adc esp_timer
                    INCLUDE_DIRS "include")