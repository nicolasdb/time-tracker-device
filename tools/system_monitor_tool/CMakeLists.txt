# system_monitor_tool CMakeLists.txt
# MCP-Inspired System Monitor Tool Build Configuration

idf_component_register(
    SRCS "system_monitor_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_common
        esp_timer
        freertos
        esp_event
)