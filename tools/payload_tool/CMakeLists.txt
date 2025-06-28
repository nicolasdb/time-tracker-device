# payload_tool CMakeLists.txt
# MCP-Inspired Payload Tool Build Configuration

idf_component_register(
    SRCS "payload_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_common
        esp_timer
        esp_event
        nvs_flash
        json
        log
    PRIV_REQUIRES
        ntp_tool
        fs_tool
        http_tool
)