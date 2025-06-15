idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        freertos
        log
        nvs_flash
        feedback_tool
        wifi_tool
        rfid_tool
)