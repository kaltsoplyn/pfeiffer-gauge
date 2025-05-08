idf_component_register(SRCS "pfeiffer_gauge.c"
                    PRIV_REQUIRES spi_flash
                    REQUIRES driver esp_adc uart_handler 
                            pressure_meas_comp 
                            temp_meas_comp 
                            lvgl_display 
                            network_comp 
                            internal_temp_sensor 
                            app_manager
                            sensor_types
                    INCLUDE_DIRS ".")
