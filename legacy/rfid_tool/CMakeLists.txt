# Add RC522 embedded component source files (exclude examples and tests)
set(rc522_srcs
    "rc522/src/rc522.c"
    "rc522/src/rc522_helpers.c"
    "rc522/src/rc522_pcd.c"
    "rc522/src/rc522_picc.c"
    "rc522/src/picc/rc522_mifare.c"
    "rc522/src/picc/rc522_nxp.c"
    "rc522/src/rc522_driver.c"
    "rc522/src/driver/rc522_spi.c"
    "rc522/src/driver/rc522_i2c.c"
)

# Set include directories explicitly
set(include_dirs "include" "rc522/include")
set(priv_include_dirs "rc522/internal")

idf_component_register(
    SRCS "rfid_tool.c" ${rc522_srcs}
    INCLUDE_DIRS ${include_dirs}
    PRIV_INCLUDE_DIRS ${priv_include_dirs}
    REQUIRES 
        freertos 
        log 
        driver
        esp_event
        esp_system
    PRIV_REQUIRES
        esp_common
        esp_timer
)

# Add compile options to ensure internal headers are found
target_compile_options(${COMPONENT_LIB} PRIVATE
    -I${CMAKE_CURRENT_SOURCE_DIR}/rc522/internal
)

# Alternative approach: Add individual source directory includes
target_include_directories(${COMPONENT_LIB} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/rc522/internal
)