idf_component_register(
    SRCS "feedback_tool_simple.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        log 
        driver
)