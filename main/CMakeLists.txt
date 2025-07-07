idf_component_register(
    SRCS "main.c" "tool_registry.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_timer
        esp_event
        nvs_flash
        system_monitor_tool
        smart_contracts_tool
        fs_tool
        feedback_tool
        network_tool
        ntp_tool
        rfid_tool
        payload_tool
        test_sequencer_tool
)