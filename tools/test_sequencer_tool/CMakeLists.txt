# Constitutional Test Sequencer Tool - CMakeLists.txt
# Tool integration testing with constitutional patterns

idf_component_register(
    SRCS "test_sequencer_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_common
        esp_timer
        freertos
        esp_event
        system_monitor_tool
        smart_contracts_tool
        fs_tool
        feedback_tool
)