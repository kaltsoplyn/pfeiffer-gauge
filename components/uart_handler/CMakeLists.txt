idf_component_register(SRCS "src/uart_handler.c"
                    REQUIRES esp_driver_uart esp_driver_gpio
                    INCLUDE_DIRS "include")