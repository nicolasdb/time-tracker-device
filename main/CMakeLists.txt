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
        fs_tool
        webhook_tool
)