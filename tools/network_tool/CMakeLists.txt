idf_component_register(
    SRCS "network_tool.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "esp_wifi" "esp_netif" "esp_event" "nvs_flash" "json"
    REQUIRES "fs_tool"
)