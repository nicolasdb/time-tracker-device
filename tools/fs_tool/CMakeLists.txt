# Constitutional FS Tool - CMakeLists.txt
# Filesystem management tool with constitutional patterns

idf_component_register(
    SRCS "fs_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_common
        esp_timer
        freertos
        esp_event
        json
        esp_partition
        wear_levelling
        vfs
        littlefs
)