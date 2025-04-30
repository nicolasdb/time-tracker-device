idf_component_register(
    SRCS "rfid_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_event rc522 esp_timer
)