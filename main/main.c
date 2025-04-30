#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "esp_efuse.h"
#include "esp_mac.h"
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
    
    // Initialize NVS for WiFi - this is required
    ESP_LOGI(TAG, "Initializing NVS for WiFi");
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);
    ESP_LOGI(TAG, "NVS initialized successfully");
    
    // Phase 2 & 3: Initialize and start WiFi Manager
    ESP_LOGI(TAG, "Initializing WiFi Manager");
    
    // Initialize the WiFi manager
    esp_err_t result = wifi_manager_init(WIFI_JSON_PATH);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
        // Continue anyway, the AP mode will still work
    }
    
    // Start the WiFi manager
    // This will:
    // 1. Try to connect to known networks
    // 2. If no connection is possible, start in AP mode
    // 3. In AP mode, serve a web page for configuration
    result = wifi_manager_start();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi manager");
    }
    
    // Phase 5: Sync time with NTP if WiFi is connected
    if (wifi_manager_is_connected()) {
        ESP_LOGI(TAG, "WiFi connected, syncing time with NTP...");
        
        // Sync time with NTP server
        if (wifi_manager_sync_time() == ESP_OK) {
            // Get and print the current time
            char time_str[64];
            wifi_manager_get_formatted_time(time_str, sizeof(time_str));
            ESP_LOGI(TAG, "Current time: %s", time_str);
        } else {
            ESP_LOGE(TAG, "Failed to sync time with NTP");
        }
    }
    
    // Get Device ID similar to your original function
    uint8_t chipid[6];
    esp_efuse_mac_get_default(chipid);
    unsigned int unique_id = ((unsigned int)chipid[0] << 16) | 
                             ((unsigned int)chipid[1] << 8) | chipid[2];
    char device_id_str[20];
    snprintf(device_id_str, sizeof(device_id_str), "NFC_%06X", unique_id);
    ESP_LOGI(TAG, "Device ID: %s", device_id_str);
    
    // Main loop
    int count = 0;
    while (1) {
        // Status update every 10 seconds
        if (count % 10 == 0) {
            ESP_LOGI(TAG, "=================== STATUS UPDATE ===================");
            
            // WiFi status
            if (wifi_manager_is_connected()) {
                char ip_str[16];
                wifi_manager_get_ip(ip_str, sizeof(ip_str));
                int8_t rssi;
                wifi_manager_get_rssi(&rssi);
                
                ESP_LOGI(TAG, "WiFi: Connected | IP: %s | RSSI: %d dBm", 
                         ip_str, rssi);
                
                // Time status
                if (wifi_manager_is_time_synced()) {
                    char time_str[64];
                    wifi_manager_get_formatted_time(time_str, sizeof(time_str));
                    ESP_LOGI(TAG, "Time: Synchronized | Local time: %s", time_str);
                } else {
                    ESP_LOGI(TAG, "Time: Not synchronized");
                    // Try to sync if not done yet
                    wifi_manager_sync_time();
                }
            } else {
                ESP_LOGI(TAG, "WiFi: Disconnected");
            }
            
            // Device info
            ESP_LOGI(TAG, "Device: ID: %s | Free heap: %u bytes", 
                     device_id_str, (unsigned int)esp_get_free_heap_size());
            
            ESP_LOGI(TAG, "====================================================");
        }
        
        count++;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}