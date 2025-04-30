#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "esp_efuse.h"
#include "esp_mac.h"
#include "esp_http_client.h"
#include "rfid_manager.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include <sys/stat.h>
#include <dirent.h>

#define TAG "time-tracker"
#define RFID_TAG "rfid"
#define MOUNT_POINT "/littlefs"
#define WIFI_JSON_PATH "/littlefs/wifi.json"
#define BUFFER_SIZE 1024
#define WEBHOOK_URL "YOUR_WEBHOOK_URL"  // Replace with your actual webhook URL
#define MAX_HTTP_RETRIES 2              // Maximum retry attempts for webhook send
#define STATUS_LED_PIN 8

// RFID tag event handlers
static rfid_manager_handle_t rfid_handle = NULL;
static bool tag_present = false;
static char last_tag_uid[32] = {0};

// HTTP client event handler
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP Client Error");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP Client Connected");
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP Client Finished");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Simplified webhook function to reduce code size
static void send_webhook(const char* event_type, const char* tag_uid) {
    if (!wifi_manager_is_connected()) {
        return;
    }
    
    char device_id[16];
    rfid_manager_get_device_uid(device_id, sizeof(device_id));
    
    char post_data[192];
    snprintf(post_data, sizeof(post_data), 
            "{\"event\":\"%s\",\"tag_uid\":\"%s\",\"device_id\":\"%s\",\"timestamp\":%lu}",
            event_type, tag_uid, device_id, (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    
    esp_http_client_config_t config = {
        .url = WEBHOOK_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return;
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

// Handler for RFID tag detection events
static void tag_detected_handler(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    rfid_tag_event_t* event = (rfid_tag_event_t*)data;
    
    // Set LED on
    gpio_set_level(STATUS_LED_PIN, 1);
    
    // Convert UID to string
    char uid_str[32] = {0};
    rfid_manager_tag_uid_to_string(&event->tag, uid_str, sizeof(uid_str));
    
    // Store tag UID for later reference
    strcpy(last_tag_uid, uid_str);
    tag_present = true;
    
    ESP_LOGI(RFID_TAG, "TAG: %s", uid_str);
    
    // Send webhook in background
    send_webhook("tag_placed", uid_str);
}

// Handler for tag removal events
static void tag_removed_handler(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    // Set LED off
    gpio_set_level(STATUS_LED_PIN, 0);
    tag_present = false;
    
    ESP_LOGI(RFID_TAG, "TAG REMOVED");
    
    // Send webhook
    send_webhook("tag_removed", last_tag_uid);
}

void app_main(void) {
    // Wait 2 seconds to ensure serial monitor is connected
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // Initialize the ESP-IDF logging
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set(RFID_TAG, ESP_LOG_INFO);
    
    // Print Hello World
    ESP_LOGI(TAG, "Hello World from ESP32-C3 Time Tracker Device!");
    
    // Initialize LED pin
    gpio_reset_pin(STATUS_LED_PIN);
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    
    // Quick blink pattern to show program is running
    for (int i = 0; i < 3; i++) {
        gpio_set_level(STATUS_LED_PIN, 1);  // LED on
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(STATUS_LED_PIN, 0);  // LED off
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
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
    
    // Initialize WiFi subsystems first - only initialize these components once
    ESP_LOGI(TAG, "Initializing WiFi subsystems");
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network interface: %s", esp_err_to_name(err));
    }
    
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
    } else if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Event loop already created");
    }
    
    // Create default station and AP netif interfaces
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create station interface");
    }
    
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create AP interface");
    }
    
    // Initialize WiFi with default configuration
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t wifi_err = esp_wifi_init(&wifi_config);
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(wifi_err));
    } else {
        ESP_LOGI(TAG, "WiFi initialized successfully");
        
        // Phase 2 & 3: Initialize and start WiFi Manager
        ESP_LOGI(TAG, "Initializing WiFi Manager");
        
        // Initialize the WiFi manager
        esp_err_t result = wifi_manager_init(WIFI_JSON_PATH);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s", esp_err_to_name(result));
            // Don't continue with WiFi manager functionality
        } else {
            // Start the WiFi manager
            // This will:
            // 1. Try to connect to known networks
            // 2. If no connection is possible, start in AP mode
            // 3. In AP mode, serve a web page for configuration
            result = wifi_manager_start();
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start WiFi manager: %s", esp_err_to_name(result));
            }
        }
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
    
    // Get Device ID
    char device_id_str[20];
    rfid_manager_get_device_uid(device_id_str, sizeof(device_id_str));
    ESP_LOGI(TAG, "Device ID: %s", device_id_str);
    
    // Phase 6: Initialize RFID Manager
    ESP_LOGI(RFID_TAG, "Initializing RFID Manager");
    rfid_handle = rfid_manager_init();
    if (rfid_handle == NULL) {
        ESP_LOGE(RFID_TAG, "Failed to initialize RFID manager");
    } else {
        // Register event handlers
        ESP_LOGI(RFID_TAG, "Registering event handlers");
        esp_err_t ret = rfid_manager_register_event_handler(rfid_handle, 
                                                          RFID_EVENT_TAG_DETECTED, 
                                                          tag_detected_handler, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(RFID_TAG, "Failed to register tag detection handler");
        }
        
        ret = rfid_manager_register_event_handler(rfid_handle, 
                                                RFID_EVENT_TAG_REMOVED, 
                                                tag_removed_handler, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(RFID_TAG, "Failed to register tag removal handler");
        }
        
        // Start scanning for tags
        ESP_LOGI(RFID_TAG, "Starting RFID scanning");
        ret = rfid_manager_start_scanning(rfid_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(RFID_TAG, "Failed to start RFID scanning");
        } else {
            ESP_LOGI(RFID_TAG, "RFID scanning started successfully");
        }
    }
    
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
            
            // RFID status
            if (rfid_handle != NULL) {
                ESP_LOGI(TAG, "RFID: Active | Tag present: %s | Last Tag: %s", 
                         tag_present ? "Yes" : "No",
                         tag_present || strlen(last_tag_uid) > 0 ? last_tag_uid : "None");
            } else {
                ESP_LOGI(TAG, "RFID: Not initialized");
            }
            
            // Device info
            ESP_LOGI(TAG, "Device: ID: %s | Free heap: %u bytes", 
                     device_id_str, (unsigned int)esp_get_free_heap_size());
            
            ESP_LOGI(TAG, "====================================================");
        }
        
        // Update LED based on state
        if (wifi_manager_is_connected()) {
            if (!tag_present) {
                // Blink slowly when WiFi connected but no tag
                gpio_set_level(STATUS_LED_PIN, count % 2 == 0);
            }
            // When tag present, LED is solid on (handled in tag handler)
        } else {
            // Fast blink when WiFi not connected
            gpio_set_level(STATUS_LED_PIN, count % 4 < 2);
        }
        
        count++;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}