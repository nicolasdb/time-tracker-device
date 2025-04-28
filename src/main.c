#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "time-tracker";

void app_main(void) {
    // Initialize the ESP-IDF logging
    esp_log_level_set(TAG, ESP_LOG_INFO);
    
    // Print Hello World
    printf("Hello World from ESP32-C3 Time Tracker Device!\n");
    ESP_LOGI(TAG, "Hello World from ESP32-C3 Time Tracker Device!");
    
    // Main loop
    int count = 0;
    while (1) {
        printf("Running... count: %d\n", count++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for 1 second
    }
}