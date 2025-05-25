idf_component_register(SRCS "src/button_comp.c"
                    REQUIRES esp_driver_gpio app_manager
                    INCLUDE_DIRS "include")