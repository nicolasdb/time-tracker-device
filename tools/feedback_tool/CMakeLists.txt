# Constitutional Feedback Tool - CMakeLists.txt
# LED visual feedback tool with constitutional patterns

idf_component_register(
    SRCS "feedback_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_common
        esp_timer
        freertos
        esp_event
        driver
        led_strip
)