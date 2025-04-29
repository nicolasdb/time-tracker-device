#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "fs_manager.h"
#include <sys/stat.h>
#include <dirent.h>

#define TAG "time-tracker"
#define MOUNT_POINT "/littlefs"
#define WIFI_JSON_PATH "/littlefs/wifi.json"
#define BUFFER_SIZE 1024

void app_main(void) {
    // Wait 2 seconds to ensure serial monitor is connected
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // Initialize the ESP-IDF logging
    esp_log_level_set(TAG, ESP_LOG_INFO);
    
    // Print Hello World
    ESP_LOGI(TAG, "Hello World from ESP32-C3 Time Tracker Device!");
    
    // Phase 1: Initialize LittleFS
    esp_err_t ret = fs_manager_init(MOUNT_POINT, NULL, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LittleFS");
        return;
    }
    
    // Get filesystem info
    size_t total_bytes = 0, used_bytes = 0;
    ret = fs_manager_info(NULL, &total_bytes, &used_bytes);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Partition size - total: %d bytes, used: %d bytes, free: %d bytes", 
                total_bytes, used_bytes, total_bytes - used_bytes);
    }
    
    // List root directory contents
    ESP_LOGI(TAG, "Listing directory: %s", MOUNT_POINT);
    
    DIR* dir = opendir(MOUNT_POINT);
    if (dir != NULL) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            struct stat st;
            char fullpath[512]; // Increased buffer size
            snprintf(fullpath, sizeof(fullpath), "%s/%s", MOUNT_POINT, entry->d_name);
            
            if (stat(fullpath, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    ESP_LOGI(TAG, "[DIR] %s", entry->d_name);
                } else {
                    ESP_LOGI(TAG, "[FILE] %s (%ld bytes)", entry->d_name, st.st_size);
                }
            }
        }
        closedir(dir);
    } else {
        ESP_LOGE(TAG, "Failed to open directory");
    }
    
    // Check if the wifi.json file exists
    struct stat st;
    if (stat(WIFI_JSON_PATH, &st) == 0) {
        ESP_LOGI(TAG, "Found wifi.json file");
        
        // Read the file content
        FILE* f = fopen(WIFI_JSON_PATH, "r");
        if (f != NULL) {
            char buffer[BUFFER_SIZE];
            size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
            
            // Null-terminate the buffer
            buffer[bytes_read] = '\0';
            
            // Display the file content
            ESP_LOGI(TAG, "wifi.json content (%d bytes):", bytes_read);
            ESP_LOGI(TAG, "%s", buffer);
            
            fclose(f);
        } else {
            ESP_LOGE(TAG, "Failed to open wifi.json for reading");
        }
    } else {
        ESP_LOGW(TAG, "wifi.json not found");
    }
    
    // Main loop
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "Running... count: %d", count++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}