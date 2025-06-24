# debug_tool CMakeLists.txt
# MCP-Inspired Debug Tool Build Configuration

idf_component_register(
    SRCS "debug_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_common
        esp_timer
        freertos
)