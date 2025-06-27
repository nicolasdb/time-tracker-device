idf_component_register(
    SRCS "http_tool.c"
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
        network_tool
        rfid_tool
        fs_tool
)