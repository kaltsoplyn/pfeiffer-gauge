idf_component_register(SRCS "src/network_comp.c" "src/wifi_manager.c" "src/web_server.c" "src/nvs_storage.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_server esp_wifi nvs_flash json app_manager)