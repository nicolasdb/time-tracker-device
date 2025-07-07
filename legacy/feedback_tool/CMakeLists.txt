idf_component_register(
    SRCS "feedback_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        log 
        driver
        esp_event
        json
)