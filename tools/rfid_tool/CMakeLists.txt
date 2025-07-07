idf_component_register(
    SRCS "rfid_tool.c"
         "rc522/src/rc522.c"
         "rc522/src/rc522_driver.c"
         "rc522/src/rc522_pcd.c"
         "rc522/src/rc522_picc.c"
         "rc522/src/rc522_helpers.c"
         "rc522/src/driver/rc522_spi.c"
         "rc522/src/picc/rc522_mifare.c"
         "rc522/src/picc/rc522_nxp.c"
    INCLUDE_DIRS "include" 
                 "rc522/include"
                 "rc522/internal"
    REQUIRES
        esp_timer
        esp_event
        freertos
        log
        fs_tool
        driver
        spi_flash
)