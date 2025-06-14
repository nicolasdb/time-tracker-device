idf_component_register(
    SRCS "wifi_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        log 
        driver
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        json
    PRIV_REQUIRES
        esp_common
)