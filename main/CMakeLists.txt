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

# Create LittleFS partition image from data directory  
littlefs_create_partition_image(storage ../data FLASH_IN_PROJECT)