#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_err.h"

#define MAX_WIFI_NETWORKS 5
#define MAX_SSID_LENGTH 32
#define MAX_PWD_LENGTH 64
#define CONFIG_FILE_PATH "/spiffs/wifi_config.txt"
#define LINE_BUFFER_SIZE 128

static const char *TAG = "time-tracker";

// Test configuration with sample WiFi credentials
const char* test_wifi_config = "HomeWifi;secret1\nOfficeNet;secret2";

typedef struct {
    char ssid[MAX_SSID_LENGTH];
    char password[MAX_PWD_LENGTH];
} wifi_network_t;

typedef struct {
    wifi_network_t networks[MAX_WIFI_NETWORKS];
    int count;
} wifi_config_t;

wifi_config_t wifi_config;

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

void create_test_wifi_config(void) {
    FILE* f = fopen(CONFIG_FILE_PATH, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to create test WiFi config file");
        return;
    }
    
    fprintf(f, "%s", test_wifi_config);
    fclose(f);
    ESP_LOGI(TAG, "Created test WiFi config file at %s", CONFIG_FILE_PATH);
}

void read_wifi_config(void) {
    FILE *file = fopen(CONFIG_FILE_PATH, "r");
    if (file == NULL) {
        ESP_LOGW(TAG, "Failed to open %s for reading", CONFIG_FILE_PATH);
        ESP_LOGI(TAG, "Creating test WiFi config file...");
        create_test_wifi_config();
        file = fopen(CONFIG_FILE_PATH, "r");
        if (file == NULL) {
            ESP_LOGE(TAG, "Still cannot open config file after creation");
            return;
        }
    }

    ESP_LOGI(TAG, "Reading WiFi configuration from %s", CONFIG_FILE_PATH);
    
    // Initialize wifi_config
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    
    char line[LINE_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file) != NULL && wifi_config.count < MAX_WIFI_NETWORKS) {
        // Remove newline character
        char *pos = strchr(line, '\n');
        if (pos) {
            *pos = '\0';
        }
        
        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }
        
        // Parse SSID and password (expected format: SSID;PASSWORD)
        char *delimiter = strchr(line, ';');
        if (delimiter) {
            *delimiter = '\0'; // Split the string at the delimiter
            
            // Copy SSID and password
            strncpy(wifi_config.networks[wifi_config.count].ssid, line, MAX_SSID_LENGTH - 1);
            strncpy(wifi_config.networks[wifi_config.count].password, delimiter + 1, MAX_PWD_LENGTH - 1);
            
            ESP_LOGI(TAG, "Found WiFi credentials:");
            ESP_LOGI(TAG, "SSID: %s, PASSWORD: %s", 
                    wifi_config.networks[wifi_config.count].ssid, 
                    wifi_config.networks[wifi_config.count].password);
            
            wifi_config.count++;
        } else {
            ESP_LOGW(TAG, "Ignoring invalid line (no ';' separator): %s", line);
        }
    }
    
    fclose(file);
    
    if (wifi_config.count == 0) {
        ESP_LOGW(TAG, "No valid WiFi credentials found in %s", CONFIG_FILE_PATH);
    } else {
        ESP_LOGI(TAG, "Successfully read %d WiFi networks from configuration", wifi_config.count);
    }
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
    } else {
        // Phase 2: Read WiFi configuration from SPIFFS
        read_wifi_config();
    }
    
    // Main loop
    int count = 0;
    while (1) {
        printf("Running... count: %d\n", count++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}