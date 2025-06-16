idf_component_register(
    SRCS "fs_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        log 
        esp_littlefs
        esp_event
        esp_system
        json
        vfs
    PRIV_REQUIRES
        esp_partition
)