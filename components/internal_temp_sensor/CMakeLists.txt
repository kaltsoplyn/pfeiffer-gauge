idf_component_register(SRCS "internal_temp_sensor.c"
                    REQUIRES esp_driver_tsens app_manager freertos sensor_types
                    INCLUDE_DIRS "include")