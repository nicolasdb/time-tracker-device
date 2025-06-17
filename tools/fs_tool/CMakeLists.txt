idf_component_register(
    SRCS "fs_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        log 
        joltwallet__littlefs
        esp_event
        esp_system
        json
        vfs
    PRIV_REQUIRES
        esp_partition
)