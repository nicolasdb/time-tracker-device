idf_component_register(
    SRCS "event_system.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_event
        log
        feedback_tool
        payload_tool
)