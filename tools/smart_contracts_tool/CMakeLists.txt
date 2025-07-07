# Constitutional Smart Contracts Tool - CMakeLists.txt
# Process map compliance validation framework (NOT blockchain)

idf_component_register(
    SRCS "smart_contracts_tool.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_common
        esp_timer
        freertos
        esp_event
)