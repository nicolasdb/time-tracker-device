idf_component_register(
    SRCS "webhook_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        log 
        esp_http_client 
        json 
        joltwallet__littlefs
        esp_event
        esp_wifi
    PRIV_REQUIRES
        wifi_tool
        rfid_tool
)