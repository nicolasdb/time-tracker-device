#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "time-tracker";

void app_main(void) {
    // Wait 2 seconds to ensure serial monitor is connected
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // Initialize the ESP-IDF logging
    esp_log_level_set(TAG, ESP_LOG_INFO);
    
    // Print Hello World - slower with pauses
    printf("Hello World from ESP32-C3 Time Tracker Device!\n");
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "Hello World from ESP32-C3 Time Tracker Device!");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    printf("Phase 0 complete - basic project setup successful\n");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    // Main loop
    int count = 0;
    while (1) {
        printf("Running... count: %d\n", count++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for 1 second
    }
}