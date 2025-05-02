idf_component_register(
    SRCS "feedback_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer freertos
)
