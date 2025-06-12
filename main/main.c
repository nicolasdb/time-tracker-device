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
#include "rfid_manager.h"
#include "webhook_manager.h"
#include "feedback_manager.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include <sys/stat.h>
#include <dirent.h>
#include "sdkconfig.h"

#define TAG "time-tracker"
#define RFID_TAG "rfid"
#define MOUNT_POINT "/littlefs"
#define WIFI_JSON_PATH "/littlefs/wifi.json"
#define WEBHOOK_CONFIG_PATH "/littlefs/webhook_config.json"
#define WEBHOOK_LOG_PATH "/littlefs/log.json"
#define STATUS_LED_PIN CONFIG_FEEDBACK_LED_GPIO

// Handles and states
static rfid_manager_handle_t rfid_handle = NULL;
static webhook_manager_handle_t webhook_handle = NULL;
static feedback_manager_handle_t feedback_handle = NULL;
static bool tag_present = false;
static char last_tag_uid[32] = {0};
static TaskHandle_t webhook_task_handle = NULL;

// Task to periodically process pending webhook events
void webhook_task(void *pvParameters) {
    // Wait a bit to let the system stabilize
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    // Load the event log in the background
    if (webhook_handle != NULL) {
        ESP_LOGI(TAG, "Loading webhook logs in background task");
        webhook_manager_load_log_file(webhook_handle);
    }
    
    // Main task loop
    while (1) {
        if (webhook_handle != NULL) {
            // Check connectivity every 30 seconds when WiFi is available
            static uint32_t last_connectivity_check = 0;
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            if (wifi_manager_is_connected() && (now - last_connectivity_check > 30000)) {
                ESP_LOGI(TAG, "Checking webhook server connectivity...");
                webhook_manager_check_connectivity(webhook_handle);
                last_connectivity_check = now;
            }
            
            // Process any pending webhook events when WiFi is available
            if (wifi_manager_is_connected()) {
                webhook_manager_process_pending(webhook_handle);
            }
        }
        
        // Check every 10 seconds
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL); // Should never reach here
}

// Handler for RFID tag detection events
static void tag_detected_handler(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    rfid_tag_event_t* event = (rfid_tag_event_t*)data;
    
    // Update feedback manager
    if (feedback_handle != NULL) {
        feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_TAG_DETECTED);
    } else {
        // Fallback for direct LED control
        gpio_set_level(STATUS_LED_PIN, 1);
    }
    
    // Convert UID to string
    char uid_str[32] = {0};
    rfid_manager_tag_uid_to_string(&event->tag, uid_str, sizeof(uid_str));
    
    // Store tag UID for later reference
    strcpy(last_tag_uid, uid_str);
    tag_present = true;
    
    ESP_LOGI(RFID_TAG, "TAG: %s", uid_str);
    
    // Get tag type string based on tag type
    char tag_type_str[16] = "unknown";
    switch (event->tag.type) {
        case 0:
            strcpy(tag_type_str, "MIFARE_1K");
            break;
        case 1:
            strcpy(tag_type_str, "MIFARE_4K");
            break;
        case 2:
            strcpy(tag_type_str, "MIFARE_UL");
            break;
        default:
            strcpy(tag_type_str, "unknown");
            break;
    }
    
    // Send webhook via the webhook manager
    if (webhook_handle != NULL) {
        webhook_manager_send_event(webhook_handle, WEBHOOK_EVENT_TAG_PLACED, uid_str, tag_type_str);
    }
}

// Handler for tag removal events
static void tag_removed_handler(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    tag_present = false;
    
    // Update feedback manager
    if (feedback_handle != NULL) {
        // Reset to background state
        feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_IDLE);
        // No flash needed - tag removal is normal operation
    } else {
        // Fallback for direct LED control
        gpio_set_level(STATUS_LED_PIN, 0);
    }
    
    ESP_LOGI(RFID_TAG, "TAG REMOVED");
    
    // Send webhook via the webhook manager
    if (webhook_handle != NULL && strlen(last_tag_uid) > 0) {
        webhook_manager_send_event(webhook_handle, WEBHOOK_EVENT_TAG_REMOVED, last_tag_uid, NULL);
    }
}

void app_main(void) {
    // Wait 2 seconds to ensure serial monitor is connected
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // Initialize the ESP-IDF logging
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set(RFID_TAG, ESP_LOG_INFO);
    
    // Print Hello World
    ESP_LOGI(TAG, "Hello World from ESP32-C3 Time Tracker Device!");
    
    // Begin initialization sequence - Boot start
    ESP_LOGI(TAG, "---- INITIALIZATION SEQUENCE START ----");
    ESP_LOGI(TAG, "Stage 1: Starting boot sequence");
    
    // Initialize Feedback Manager with WS2812B support
    ESP_LOGI(TAG, "Initializing Feedback Manager");
    feedback_handle = feedback_manager_init(STATUS_LED_PIN);
    if (feedback_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize feedback manager, falling back to basic LED");
        
        // Initialize LED pin for backward compatibility
        gpio_reset_pin(STATUS_LED_PIN);
        gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
        
        // Quick blink pattern to show program is running (fallback mode)
        for (int i = 0; i < 3; i++) {
            gpio_set_level(STATUS_LED_PIN, 1);  // LED on
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED_PIN, 0);  // LED off
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    } else {
        ESP_LOGI(TAG, "Feedback Manager initialized successfully");
        feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_INIT_START);
        vTaskDelay(pdMS_TO_TICKS(500)); // Display initial state briefly
    }
    
    // Phase 1: Initialize LittleFS
    ESP_LOGI(TAG, "Stage 2: Initializing filesystem");
    if (feedback_handle != NULL) {
        feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_INIT_FS);
    }
    
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
        
        // Show error state on LED
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, false);
        }
        return;
    }
    
    ESP_LOGI(TAG, "LittleFS mounted successfully");
    
    // Show success on LED
    if (feedback_handle != NULL) {
        feedback_manager_validate_init_step(feedback_handle, true);
    }
    
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
        if (feedback_handle != NULL) {
            feedback_manager_flash_event(feedback_handle, FEEDBACK_STATE_ERROR, 2);
        }
    }
    
    // Initialize NVS for WiFi - this is required
    ESP_LOGI(TAG, "Stage 3: Initializing NVS for WiFi");
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    
    if (nvs_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(nvs_ret));
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, false);
        }
        return;
    }
    
    ESP_LOGI(TAG, "NVS initialized successfully");
    if (feedback_handle != NULL) {
        feedback_manager_validate_init_step(feedback_handle, true);
    }
    
    // Stage 4: Initialize WiFi subsystems
    ESP_LOGI(TAG, "Stage 4: Initializing WiFi subsystems");
    if (feedback_handle != NULL) {
        feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_INIT_WIFI_PREP);
    }
    
    // Initialize WiFi subsystems first - only initialize these components once
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network interface: %s", esp_err_to_name(err));
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, false);
        }
    }
    
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, false);
        }
    } else if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Event loop already created");
    }
    
    // Create default station and AP netif interfaces
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create station interface");
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, false);
        }
    }
    
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create AP interface");
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, false);
        }
    }
    
    // Initialize WiFi with default configuration
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t wifi_err = esp_wifi_init(&wifi_config);
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(wifi_err));
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, false);
        }
    } else {
        ESP_LOGI(TAG, "WiFi initialized successfully");
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, true);
        }
        
        // Stage 5: Connect to WiFi (or start AP mode)
        ESP_LOGI(TAG, "Stage 5: Connecting to WiFi");
        if (feedback_handle != NULL) {
            feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_WIFI_CONNECTING);
        }
        
        // Initialize the WiFi manager
        esp_err_t result = wifi_manager_init(WIFI_JSON_PATH);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s", esp_err_to_name(result));
            // Don't continue with WiFi manager functionality
            if (feedback_handle != NULL) {
                feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_WIFI_FAILED);
                vTaskDelay(pdMS_TO_TICKS(1000)); // Show error state briefly
            }
        } else {
            // Start the WiFi manager
            // This will:
            // 1. Try to connect to known networks
            // 2. If no connection is possible, start in AP mode
            // 3. In AP mode, serve a web page for configuration
            result = wifi_manager_start();
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start WiFi manager: %s", esp_err_to_name(result));
                if (feedback_handle != NULL) {
                    feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_WIFI_FAILED);
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Show error state briefly
                }
            } else if (wifi_manager_is_in_ap_mode()) {
                ESP_LOGI(TAG, "WiFi manager started in AP mode");
                if (feedback_handle != NULL) {
                    feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_WIFI_AP_MODE);
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Show AP mode state briefly
                }
            } else if (wifi_manager_is_connected()) {
                ESP_LOGI(TAG, "WiFi connected successfully");
                if (feedback_handle != NULL) {
                    feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_WIFI_CONNECTED);
                    vTaskDelay(pdMS_TO_TICKS(500)); // Show connected state briefly
                }
            }
        }
    }
    
    // Stage 6: Sync time with NTP if WiFi is connected
    if (wifi_manager_is_connected()) {
        ESP_LOGI(TAG, "Stage 6: Synchronizing time with NTP");
        if (feedback_handle != NULL) {
            feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_INIT_TIME);
        }
        
        // Sync time with NTP server
        if (wifi_manager_sync_time() == ESP_OK) {
            // Get and print the current time
            char time_str[64];
            wifi_manager_get_formatted_time(time_str, sizeof(time_str));
            ESP_LOGI(TAG, "Current time: %s", time_str);
            
            if (feedback_handle != NULL) {
                feedback_manager_validate_init_step(feedback_handle, true);
            }
        } else {
            ESP_LOGE(TAG, "Failed to sync time with NTP");
            if (feedback_handle != NULL) {
                feedback_manager_validate_init_step(feedback_handle, false);
            }
        }
    }
    
    // Get Device ID (now using full MAC address)
    char device_id_str[20];
    rfid_manager_get_device_uid(device_id_str, sizeof(device_id_str));
    ESP_LOGI(TAG, "Device ID: %s", device_id_str);
    
    // Stage 7: Initialize Webhook Manager
    ESP_LOGI(TAG, "Stage 7: Initializing Webhook Manager");
    if (feedback_handle != NULL) {
        feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_INIT_WEBHOOK);
    }
    
    webhook_handle = webhook_manager_init(WEBHOOK_CONFIG_PATH, WEBHOOK_LOG_PATH);
    if (webhook_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize webhook manager");
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, false);
        }
    } else {
        // Set the device ID in webhook manager
        webhook_manager_set_device_id(webhook_handle, device_id_str);
        
        // Start webhook task for processing pending webhooks with increased stack size
        xTaskCreate(webhook_task, "webhook_task", 8192, NULL, 1, &webhook_task_handle);
        
        // Store the task handle in the webhook manager
        webhook_manager_set_task_handle(webhook_handle, webhook_task_handle);
        
        // Get webhook status
        bool is_configured, is_connected;
        if (webhook_manager_get_status(webhook_handle, &is_configured, &is_connected) == ESP_OK) {
            ESP_LOGI(TAG, "Webhook status: configured=%s, connected=%s", 
                    is_configured ? "true" : "false", 
                    is_connected ? "true" : "false");
        }
        
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, true);
        }
    }

    // Configuration and logs are loaded in the webhook_task
    
    // Stage 8: Initialize RFID Manager
    ESP_LOGI(TAG, "Stage 8: Initializing RFID Manager");
    if (feedback_handle != NULL) {
        feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_INIT_RFID);
    }
    
    rfid_handle = rfid_manager_init();
    if (rfid_handle == NULL) {
        ESP_LOGE(RFID_TAG, "Failed to initialize RFID manager");
        if (feedback_handle != NULL) {
            feedback_manager_validate_init_step(feedback_handle, false);
        }
    } else {
        // Register event handlers
        ESP_LOGI(RFID_TAG, "Registering event handlers");
        esp_err_t ret = rfid_manager_register_event_handler(rfid_handle, 
                                                          RFID_EVENT_TAG_DETECTED, 
                                                          tag_detected_handler, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(RFID_TAG, "Failed to register tag detection handler");
            if (feedback_handle != NULL) {
                feedback_manager_validate_init_step(feedback_handle, false);
            }
        }
        
        ret = rfid_manager_register_event_handler(rfid_handle, 
                                                RFID_EVENT_TAG_REMOVED, 
                                                tag_removed_handler, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(RFID_TAG, "Failed to register tag removal handler");
            if (feedback_handle != NULL) {
                feedback_manager_validate_init_step(feedback_handle, false);
            }
        }
        
        // Start scanning for tags
        ESP_LOGI(RFID_TAG, "Starting RFID scanning");
        ret = rfid_manager_start_scanning(rfid_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(RFID_TAG, "Failed to start RFID scanning");
            if (feedback_handle != NULL) {
                feedback_manager_validate_init_step(feedback_handle, false);
            }
        } else {
            ESP_LOGI(RFID_TAG, "RFID scanning started successfully");
            if (feedback_handle != NULL) {
                feedback_manager_validate_init_step(feedback_handle, true);
            }
        }
    }
    
    // Stage 9: All systems initialized - show completion state
    ESP_LOGI(TAG, "---- INITIALIZATION SEQUENCE COMPLETE ----");
    if (feedback_handle != NULL) {
        feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_INIT_COMPLETE);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Show completion state
        
        // Finally, transition to idle state with appropriate background state
        if (wifi_manager_is_connected()) {
            feedback_manager_set_background_state(feedback_handle, FEEDBACK_STATE_WIFI_CONNECTED);
        } else if (wifi_manager_is_in_ap_mode()) {
            feedback_manager_set_background_state(feedback_handle, FEEDBACK_STATE_WIFI_AP_MODE);
        } else {
            feedback_manager_set_background_state(feedback_handle, FEEDBACK_STATE_WIFI_FAILED);
        }
        
        // Set to idle state - this will show the background state with breathing effect
        feedback_manager_set_state(feedback_handle, FEEDBACK_STATE_IDLE);
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
                
                // Update feedback manager with WiFi status
                if (feedback_handle != NULL) {
                    feedback_manager_set_background_state(feedback_handle, FEEDBACK_STATE_WIFI_CONNECTED);
                }
                
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
                
                // Update feedback manager with WiFi status
                if (feedback_handle != NULL) {
                    feedback_manager_set_background_state(feedback_handle, FEEDBACK_STATE_WIFI_FAILED);
                }
            }
            
            // RFID status
            if (rfid_handle != NULL) {
                // Simple check - hardware detected if handle exists
                bool rfid_hardware_ok = (rfid_handle != NULL);
                ESP_LOGI(TAG, "RFID: Active (%s) | Tag present: %s | Last Tag: %s", 
                         rfid_hardware_ok ? "Hardware OK" : "Hardware NOT DETECTED",
                         tag_present ? "Yes" : "No",
                         tag_present || strlen(last_tag_uid) > 0 ? last_tag_uid : "None");
            } else {
                ESP_LOGI(TAG, "RFID: Not initialized");
            }
            
            // Webhook status
            if (webhook_handle != NULL) {
                int pending_count = 0;
                webhook_manager_get_pending_count(webhook_handle, &pending_count);
                
                bool is_configured, is_connected;
                webhook_manager_get_status(webhook_handle, &is_configured, &is_connected);
                
                ESP_LOGI(TAG, "Webhook: %s | Connected: %s | Pending events: %d", 
                         is_configured ? "Configured" : "Not configured",
                         is_connected ? "Yes" : "No",
                         pending_count);
            } else {
                ESP_LOGI(TAG, "Webhook: Not initialized");
            }
            
            // Device info
            ESP_LOGI(TAG, "Device: ID: %s | Free heap: %u bytes", 
                     device_id_str, (unsigned int)esp_get_free_heap_size());
            
            ESP_LOGI(TAG, "====================================================");
        }
        
        // Update LED based on state (direct control)
        // Only use direct LED control if feedback manager isn't initialized
        if (feedback_handle == NULL) {
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
        }
        
        count++;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}