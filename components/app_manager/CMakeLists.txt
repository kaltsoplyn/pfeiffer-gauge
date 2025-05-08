idf_component_register(SRCS "src/app_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES freertos 
                             pressure_meas_comp # For PressureData type definition
                             temp_meas_comp     # For TemperatureData type definition
                             internal_temp_sensor # For internal temperature sensor
                             network_comp       # For wifi_is_connected and new start/stop functions
                             sensor_types       # for types
                    # Add nvs_flash to implement NVS loading/saving for config
                    )