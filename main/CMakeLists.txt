idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_client rfid_manager wifi_manager webhook_manager driver esp_littlefs nvs_flash json)