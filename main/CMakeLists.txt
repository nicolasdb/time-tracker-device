idf_component_register(
    SRCS "main.c" "tool_registry.c"
    INCLUDE_DIRS "." "include"
    REQUIRES 
        freertos
        log
        nvs_flash
        feedback_tool
        network_tool
        rfid_tool
        fs_tool
        http_tool
        webserver_tool
        ntp_tool
        debug_tool
        payload_tool
)

# Create LittleFS partition image from data directory  
littlefs_create_partition_image(storage ../data FLASH_IN_PROJECT)