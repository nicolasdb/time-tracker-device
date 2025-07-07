idf_component_register(
    SRCS "ntp_tool.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif esp_timer freertos log lwip
)