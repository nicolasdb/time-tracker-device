#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_littlefs.h"
#include "wifi_manager.h"
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
    ESP_LOGI(TAG, "Initializing LittleFS");
    
    esp_vfs_littlefs_conf_t conf = {
        .base_path = MOUNT_POINT,
        .partition_label = NULL,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    
    ESP_LOGI(TAG, "LittleFS mounted successfully");
    
    // Get filesystem info
    size_t total_bytes = 0, used_bytes = 0;
    ret = esp_littlefs_info(NULL, &total_bytes, &used_bytes);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Partition size: total: %d bytes, used: %d bytes, free: %d bytes", 
                total_bytes, used_bytes, total_bytes - used_bytes);
    }
    
    // List root directory contents
    ESP_LOGI(TAG, "Listing directory: %s", MOUNT_POINT);
    
    DIR* dir = opendir(MOUNT_POINT);
    if (dir != NULL) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            struct stat st;
            char fullpath[512];
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
    
    // Phase 2: Parse WiFi configuration from JSON
    ESP_LOGI(TAG, "Initializing WiFi Manager");
    wifi_networks_config_t wifi_networks;
    esp_err_t result = wifi_manager_parse_config(WIFI_JSON_PATH, &wifi_networks);
    
    if (result == ESP_OK) {
        // Print the parsed configuration
        wifi_manager_print_config(&wifi_networks);
        
        // At this point, we've successfully parsed the WiFi configuration
        ESP_LOGI(TAG, "Successfully parsed WiFi configuration");
    } else {
        ESP_LOGE(TAG, "Failed to parse WiFi configuration");
    }
    
    // Main loop
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "Running... count: %d", count++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}