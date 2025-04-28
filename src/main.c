#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_err.h"

static const char *TAG = "time-tracker";

esp_err_t init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "SPIFFS mounted successfully");
    ESP_LOGI(TAG, "Partition size: total: %d bytes, used: %d bytes", total, used);
    
    return ESP_OK;
}

void app_main(void) {
    // Wait 2 seconds to ensure serial monitor is connected
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // Initialize the ESP-IDF logging
    esp_log_level_set(TAG, ESP_LOG_INFO);
    
    // Print Hello World
    printf("Hello World from ESP32-C3 Time Tracker Device!\n");
    ESP_LOGI(TAG, "Hello World from ESP32-C3 Time Tracker Device!");
    
    // Phase 1: Initialize SPIFFS
    esp_err_t ret = init_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS initialization failed");
    }
    
    // Main loop
    int count = 0;
    while (1) {
        printf("Running... count: %d\n", count++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}