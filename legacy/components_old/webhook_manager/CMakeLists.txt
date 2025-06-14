idf_component_register(SRCS "webhook_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client json nvs_flash esp_littlefs freertos esp_wifi wifi_manager)