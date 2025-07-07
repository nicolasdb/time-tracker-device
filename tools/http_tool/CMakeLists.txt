idf_component_register(
    SRCS "http_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        log 
        esp_http_client 
        json 
        esp_event
        esp_wifi
        esp_timer
        payload_tool
    PRIV_REQUIRES
        network_tool
        ntp_tool
)