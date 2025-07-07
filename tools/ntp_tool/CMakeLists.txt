idf_component_register(
    SRCS "ntp_tool.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "esp_netif" "esp_event" "lwip"
    REQUIRES "network_tool"
)