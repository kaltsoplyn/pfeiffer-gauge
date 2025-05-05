idf_component_register(SRCS "lvgl_display.c"
                    REQUIRES driver esp_lcd lvgl esp_lvgl_port
                    INCLUDE_DIRS "include")