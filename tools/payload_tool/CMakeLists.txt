idf_component_register(
    SRCS "payload_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_timer
        esp_event
        freertos
        log
        json
        fs_tool
        ntp_tool
)