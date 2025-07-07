/**
 * @file network_tool.c
 * @brief Constitutional Network Tool Implementation - WiFi Connectivity
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Network connectivity foundation for ecosystem Layer 1
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#include "network_tool.h"
#include "fs_tool.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static const char* TAG = "network_tool";

// Constitutional WiFi event bits
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_FAIL_BIT         BIT1
#define WIFI_AP_STARTED_BIT   BIT2

// Constitutional Network Tool Event Base
ESP_EVENT_DEFINE_BASE(NETWORK_TOOL_EVENTS);

// Constitutional Network Tool Context (Handle-based pattern)
struct network_tool {
    bool is_initialized;
    bool is_active;
    bool wifi_hardware_ok;
    network_tool_config_t config;
    network_tool_status_t status;
    uint64_t init_timestamp_us;
    uint32_t connection_attempts;
    uint32_t error_count;
    
    // Current network state
    network_state_t current_state;
    char connected_ssid[32];
    char ip_address[16];
    int8_t rssi;
    
    // WiFi configuration from wifi.json
    char wifi_ssid[32];
    char wifi_password[64];
    bool config_loaded;
    
    // ESP32 WiFi resources
    esp_netif_t *sta_netif;
    esp_netif_t *ap_netif;
    EventGroupHandle_t wifi_event_group;
    
    // Constitutional tool dependencies
    fs_tool_handle_t fs_tool;
    
    // Constitutional task management
    TaskHandle_t connection_task_handle;
    esp_event_loop_handle_t event_loop;
};

// =============================================================================
// Constitutional WiFi Event Handlers
// =============================================================================

/**
 * @brief Constitutional WiFi event handler
 * Non-blocking event processing per constitutional patterns
 */
static void constitutional_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    network_tool_handle_t handle = (network_tool_handle_t)arg;
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in WiFi event handler");
        return;
    }
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "🔌 WiFi Station started");
                break;
                
            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
                snprintf(handle->connected_ssid, sizeof(handle->connected_ssid), "%s", (char*)event->ssid);
                handle->current_state = NETWORK_STATE_CONNECTED;
                handle->status.state = NETWORK_STATE_CONNECTED;
                snprintf(handle->status.connected_ssid, sizeof(handle->status.connected_ssid), "%s", (char*)event->ssid);
                handle->status.connection_count++;
                handle->status.last_connect_time_us = esp_timer_get_time();
                
                ESP_LOGI(TAG, "✅ Connected to WiFi: %s", handle->connected_ssid);
                
                // Set WiFi connected bit
                if (handle->wifi_event_group) {
                    xEventGroupSetBits(handle->wifi_event_group, WIFI_CONNECTED_BIT);
                }
                
                // Publish constitutional event
                network_tool_event_t net_event = {
                    .state = NETWORK_STATE_CONNECTED,
                    .timestamp_us = esp_timer_get_time()
                };
                snprintf(net_event.ssid, sizeof(net_event.ssid), "%s", handle->connected_ssid);
                esp_event_post(NETWORK_TOOL_EVENTS, NETWORK_TOOL_EVENT_CONNECTED, &net_event, sizeof(net_event), 0);
                break;
            }
            
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                handle->current_state = NETWORK_STATE_DISCONNECTED;
                handle->status.state = NETWORK_STATE_DISCONNECTED;
                handle->status.error_count++;
                
                ESP_LOGW(TAG, "❌ WiFi disconnected, reason: %" PRIu32, (uint32_t)event->reason);
                
                // Clear WiFi connected bit
                if (handle->wifi_event_group) {
                    xEventGroupClearBits(handle->wifi_event_group, WIFI_CONNECTED_BIT);
                    xEventGroupSetBits(handle->wifi_event_group, WIFI_FAIL_BIT);
                }
                
                // Publish constitutional event
                network_tool_event_t net_event = {
                    .state = NETWORK_STATE_DISCONNECTED,
                    .timestamp_us = esp_timer_get_time()
                };
                esp_event_post(NETWORK_TOOL_EVENTS, NETWORK_TOOL_EVENT_DISCONNECTED, &net_event, sizeof(net_event), 0);
                break;
            }
            
            case WIFI_EVENT_AP_START:
                handle->current_state = NETWORK_STATE_AP_MODE;
                handle->status.state = NETWORK_STATE_AP_MODE;
                ESP_LOGI(TAG, "🔥 WiFi AP mode started");
                
                // Set AP started bit
                if (handle->wifi_event_group) {
                    xEventGroupSetBits(handle->wifi_event_group, WIFI_AP_STARTED_BIT);
                }
                
                // Publish constitutional event
                network_tool_event_t net_event = {
                    .state = NETWORK_STATE_AP_MODE,
                    .timestamp_us = esp_timer_get_time()
                };
                esp_event_post(NETWORK_TOOL_EVENTS, NETWORK_TOOL_EVENT_AP_STARTED, &net_event, sizeof(net_event), 0);
                break;
                
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                snprintf(handle->ip_address, sizeof(handle->ip_address), 
                        IPSTR, IP2STR(&event->ip_info.ip));
                snprintf(handle->status.ip_address, sizeof(handle->status.ip_address), 
                        IPSTR, IP2STR(&event->ip_info.ip));
                
                ESP_LOGI(TAG, "🌐 Got IP address: %s", handle->ip_address);
                
                // Publish constitutional event
                network_tool_event_t net_event = {
                    .state = NETWORK_STATE_CONNECTED,
                    .timestamp_us = esp_timer_get_time()
                };
                snprintf(net_event.ip_address, sizeof(net_event.ip_address), "%s", handle->ip_address);
                esp_event_post(NETWORK_TOOL_EVENTS, NETWORK_TOOL_EVENT_IP_ACQUIRED, &net_event, sizeof(net_event), 0);
                break;
            }
            
            default:
                break;
        }
    }
}

// =============================================================================
// Constitutional Network Configuration
// =============================================================================

/**
 * @brief Load WiFi configuration from wifi.json using fs_tool
 */
static esp_err_t constitutional_load_wifi_config(network_tool_handle_t handle)
{
    if (!handle || !handle->fs_tool) {
        ESP_LOGE(TAG, "Invalid handle or fs_tool dependency not set");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "📖 Loading WiFi configuration from %s", "wifi.json");
    
    // CONSTITUTIONAL DEBUG: Test filesystem status and create test file
    ESP_LOGI(TAG, "🔍 DEBUG: Testing filesystem write capability");
    
    // Test if we can create a simple file (use relative path since fs_tool adds mount point)
    cJSON *test_json = cJSON_CreateObject();
    cJSON_AddStringToObject(test_json, "debug_test", "filesystem_write_test");
    esp_err_t test_ret = fs_tool_save_json_config(handle->fs_tool, "debug_test.json", test_json);
    ESP_LOGI(TAG, "🔍 DEBUG: Test file write result: %s", esp_err_to_name(test_ret));
    cJSON_Delete(test_json);
    
    // Try loading the missing wifi.json (use relative path)
    cJSON *json_config = NULL;
    esp_err_t ret = fs_tool_load_json_config(handle->fs_tool, "wifi.json", &json_config);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load WiFi configuration: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (!json_config) {
        ESP_LOGE(TAG, "WiFi configuration JSON is null");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Parse WiFi configuration
    cJSON *networks = cJSON_GetObjectItemCaseSensitive(json_config, "networks");
    if (!cJSON_IsArray(networks)) {
        ESP_LOGE(TAG, "WiFi configuration missing 'networks' array");
        cJSON_Delete(json_config);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get first network (simple 80/20 approach)
    cJSON *network = cJSON_GetArrayItem(networks, 0);
    if (!network) {
        ESP_LOGE(TAG, "No networks configured in WiFi configuration");
        cJSON_Delete(json_config);
        return ESP_ERR_INVALID_STATE;
    }
    
    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(network, "ssid");
    cJSON *password = cJSON_GetObjectItemCaseSensitive(network, "password");
    
    if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
        ESP_LOGE(TAG, "Invalid SSID or password in WiFi configuration");
        cJSON_Delete(json_config);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Store configuration with constitutional memory safety
    snprintf(handle->wifi_ssid, sizeof(handle->wifi_ssid), "%s", cJSON_GetStringValue(ssid));
    snprintf(handle->wifi_password, sizeof(handle->wifi_password), "%s", cJSON_GetStringValue(password));
    handle->config_loaded = true;
    
    ESP_LOGI(TAG, "✅ WiFi configuration loaded - SSID: %s", handle->wifi_ssid);
    
    cJSON_Delete(json_config);
    
    // Publish constitutional event
    network_tool_event_t net_event = {
        .state = handle->current_state,
        .timestamp_us = esp_timer_get_time()
    };
    snprintf(net_event.ssid, sizeof(net_event.ssid), "%s", handle->wifi_ssid);
    esp_event_post(NETWORK_TOOL_EVENTS, NETWORK_TOOL_EVENT_CONFIG_LOADED, &net_event, sizeof(net_event), 0);
    
    return ESP_OK;
}

// =============================================================================
// Constitutional Network Connection Task
// =============================================================================

/**
 * @brief Constitutional network connection task
 * Handles WiFi connection lifecycle with constitutional patterns
 */
static void constitutional_network_connection_task(void *arg)
{
    network_tool_handle_t handle = (network_tool_handle_t)arg;
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in connection task");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "🔗 Constitutional network connection task started");
    
    while (handle->is_active) {
        // Check if we need to connect
        if (handle->current_state == NETWORK_STATE_DISCONNECTED && handle->config_loaded) {
            ESP_LOGI(TAG, "🔄 Attempting WiFi connection to: %s", handle->wifi_ssid);
            
            handle->current_state = NETWORK_STATE_CONNECTING;
            handle->status.state = NETWORK_STATE_CONNECTING;
            handle->connection_attempts++;
            
            // Publish connecting event
            network_tool_event_t net_event = {
                .state = NETWORK_STATE_CONNECTING,
                .timestamp_us = esp_timer_get_time()
            };
            snprintf(net_event.ssid, sizeof(net_event.ssid), "%s", handle->wifi_ssid);
            esp_event_post(NETWORK_TOOL_EVENTS, NETWORK_TOOL_EVENT_CONNECTING, &net_event, sizeof(net_event), 0);
            
            // Configure WiFi
            wifi_config_t wifi_config = {0};
            snprintf((char*)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", handle->wifi_ssid);
            snprintf((char*)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", handle->wifi_password);
            
            esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
                handle->current_state = NETWORK_STATE_FAILED;
                handle->status.state = NETWORK_STATE_FAILED;
                handle->status.error_count++;
                
                // Publish failed event
                network_tool_event_t fail_event = {
                    .state = NETWORK_STATE_FAILED,
                    .timestamp_us = esp_timer_get_time()
                };
                esp_event_post(NETWORK_TOOL_EVENTS, NETWORK_TOOL_EVENT_FAILED, &fail_event, sizeof(fail_event), 0);
                
                // Wait before retry
                vTaskDelay(pdMS_TO_TICKS(handle->config.retry_delay_ms));
                continue;
            }
            
            // Start connection
            ret = esp_wifi_connect();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start WiFi connection: %s", esp_err_to_name(ret));
                handle->current_state = NETWORK_STATE_FAILED;
                handle->status.state = NETWORK_STATE_FAILED;
                handle->status.error_count++;
                
                // Wait before retry
                vTaskDelay(pdMS_TO_TICKS(handle->config.retry_delay_ms));
                continue;
            }
            
            // Wait for connection result
            if (handle->wifi_event_group) {
                EventBits_t bits = xEventGroupWaitBits(handle->wifi_event_group,
                                                      WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                                      pdFALSE,
                                                      pdFALSE,
                                                      pdMS_TO_TICKS(handle->config.connection_timeout_ms));
                
                if (bits & WIFI_CONNECTED_BIT) {
                    ESP_LOGI(TAG, "✅ WiFi connection successful");
                    // State already updated in event handler
                } else if (bits & WIFI_FAIL_BIT) {
                    ESP_LOGW(TAG, "❌ WiFi connection failed");
                    handle->current_state = NETWORK_STATE_FAILED;
                    handle->status.state = NETWORK_STATE_FAILED;
                    handle->status.error_count++;
                    
                    // Clear fail bit for next attempt
                    xEventGroupClearBits(handle->wifi_event_group, WIFI_FAIL_BIT);
                } else {
                    ESP_LOGW(TAG, "⏰ WiFi connection timeout");
                    handle->current_state = NETWORK_STATE_FAILED;
                    handle->status.state = NETWORK_STATE_FAILED;
                    handle->status.error_count++;
                }
            }
        }
        
        // Constitutional task yield
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "🔗 Constitutional network connection task ended");
    vTaskDelete(NULL);
}

// =============================================================================
// Constitutional Network Hardware Validation
// =============================================================================

/**
 * @brief Constitutional WiFi hardware self-test
 * Validates WiFi peripheral similar to LED self-test pattern
 */
static esp_err_t constitutional_wifi_hardware_self_test(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔧 Constitutional WiFi hardware self-test starting");
    
    // Test 1: WiFi initialization
    esp_err_t ret = esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WiFi initialization failed: %s", esp_err_to_name(ret));
        handle->wifi_hardware_ok = false;
        return ret;
    }
    
    // Test 2: WiFi mode setting
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WiFi mode setting failed: %s", esp_err_to_name(ret));
        handle->wifi_hardware_ok = false;
        return ret;
    }
    
    // Test 3: WiFi start
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WiFi start failed: %s", esp_err_to_name(ret));
        handle->wifi_hardware_ok = false;
        return ret;
    }
    
    // Test 4: Network scan capability
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,
        .scan_time = {
            .passive = 100  // Quick scan for hardware validation
        }
    };
    
    ret = esp_wifi_scan_start(&scan_config, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WiFi scan start failed: %s", esp_err_to_name(ret));
        handle->wifi_hardware_ok = false;
        return ret;
    }
    
    // Wait for scan completion (non-blocking)
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Get scan results count
    uint16_t scan_count = 0;
    ret = esp_wifi_scan_get_ap_num(&scan_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WiFi scan results failed: %s", esp_err_to_name(ret));
        handle->wifi_hardware_ok = false;
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ WiFi hardware self-test complete - Found %" PRIu32 " networks", (uint32_t)scan_count);
    handle->wifi_hardware_ok = true;
    handle->status.wifi_hardware_ok = true;
    
    return ESP_OK;
}

// =============================================================================
// Constitutional Network Tool Interface Implementation
// =============================================================================

const char* network_tool_get_id(void)
{
    return "network_tool";
}

const char* network_tool_get_version(void)
{
    return "6.1.0";
}

network_tool_config_t network_tool_create_default_config(void)
{
    network_tool_config_t config = {
        .connection_timeout_ms = 10000,
        .retry_attempts = 3,
        .retry_delay_ms = 2000,
        .enable_ap_fallback = false,
        .publish_events = true
    };
    
    // Default wifi.json path (Constitutional fix: Use relative path for fs_tool)
    snprintf(config.config_file, sizeof(config.config_file), "wifi.json");
    
    return config;
}

network_tool_handle_t network_tool_init(const network_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    ESP_LOGI(TAG, "🏗️ Constitutional network tool initializing");
    
    // Allocate handle with constitutional memory safety
    network_tool_handle_t handle = malloc(sizeof(struct network_tool));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate network tool handle");
        return NULL;
    }
    
    // Initialize handle with constitutional patterns
    memset(handle, 0, sizeof(struct network_tool));
    memcpy(&handle->config, config, sizeof(network_tool_config_t));
    handle->init_timestamp_us = esp_timer_get_time();
    handle->current_state = NETWORK_STATE_DISCONNECTED;
    handle->status.state = NETWORK_STATE_DISCONNECTED;
    
    // Initialize ESP32 WiFi subsystem
    ESP_ERROR_CHECK(esp_netif_init());
    handle->sta_netif = esp_netif_create_default_wifi_sta();
    if (!handle->sta_netif) {
        ESP_LOGE(TAG, "Failed to create WiFi station interface");
        free(handle);
        return NULL;
    }
    
    // Create WiFi event group
    handle->wifi_event_group = xEventGroupCreate();
    if (!handle->wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        free(handle);
        return NULL;
    }
    
    // Register event handlers
    esp_err_t ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                             &constitutional_wifi_event_handler, handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        vEventGroupDelete(handle->wifi_event_group);
        free(handle);
        return NULL;
    }
    
    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                   &constitutional_wifi_event_handler, handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &constitutional_wifi_event_handler);
        vEventGroupDelete(handle->wifi_event_group);
        free(handle);
        return NULL;
    }
    
    // Perform constitutional hardware self-test
    if (constitutional_wifi_hardware_self_test(handle) != ESP_OK) {
        ESP_LOGE(TAG, "Constitutional WiFi hardware self-test failed");
        // Continue initialization but mark hardware as problematic
        handle->wifi_hardware_ok = false;
        handle->status.wifi_hardware_ok = false;
    }
    
    // Set control flags BEFORE task creation (constitutional race condition fix)
    handle->is_active = true;
    handle->is_initialized = true;
    handle->status.is_initialized = true;
    handle->status.is_active = true;
    
    // Create constitutional connection task
    BaseType_t task_ret = xTaskCreate(constitutional_network_connection_task,
                                     "network_connection",
                                     4096,
                                     handle,
                                     5,
                                     &handle->connection_task_handle);
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create network connection task");
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &constitutional_wifi_event_handler);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &constitutional_wifi_event_handler);
        vEventGroupDelete(handle->wifi_event_group);
        free(handle);
        return NULL;
    }
    
    ESP_LOGI(TAG, "✅ Constitutional network tool: %s v%s initialized", 
             network_tool_get_id(), network_tool_get_version());
    
    return handle;
}

esp_err_t network_tool_deinit(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔌 Constitutional network tool deinitializing");
    
    // Stop connection task
    handle->is_active = false;
    if (handle->connection_task_handle) {
        vTaskDelete(handle->connection_task_handle);
        handle->connection_task_handle = NULL;
    }
    
    // Disconnect WiFi
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // Unregister event handlers
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &constitutional_wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &constitutional_wifi_event_handler);
    
    // Clean up resources
    if (handle->wifi_event_group) {
        vEventGroupDelete(handle->wifi_event_group);
    }
    
    // Constitutional cleanup
    free(handle);
    
    ESP_LOGI(TAG, "✅ Constitutional network tool deinitialized");
    
    return ESP_OK;
}

esp_err_t network_tool_get_status(network_tool_handle_t handle, network_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(status, &handle->status, sizeof(network_tool_status_t));
    
    return ESP_OK;
}

esp_err_t network_tool_set_fs_dependency(network_tool_handle_t handle, fs_tool_handle_t fs_tool)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->fs_tool = fs_tool;
    ESP_LOGI(TAG, "✅ FS tool dependency set for configuration loading");
    
    return ESP_OK;
}

esp_err_t network_tool_connect(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->config_loaded) {
        ESP_LOGI(TAG, "📖 Loading WiFi configuration before connection");
        esp_err_t ret = constitutional_load_wifi_config(handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load WiFi configuration: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "🔗 Initiating WiFi connection");
    
    // Connection will be handled by the connection task
    return ESP_OK;
}

esp_err_t network_tool_disconnect(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔌 Disconnecting WiFi");
    
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    handle->current_state = NETWORK_STATE_DISCONNECTED;
    handle->status.state = NETWORK_STATE_DISCONNECTED;
    
    return ESP_OK;
}

esp_err_t network_tool_start_ap(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔥 Starting AP mode (not implemented in 80/20 version)");
    
    // 80/20 rule: AP mode is nice-to-have, not implementing for proof of concept
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t network_tool_stop_ap(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔥 Stopping AP mode (not implemented in 80/20 version)");
    
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t network_tool_hardware_self_test(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return constitutional_wifi_hardware_self_test(handle);
}

esp_err_t network_tool_scan_networks(network_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔍 Scanning for available networks");
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };
    
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start network scan: %s", esp_err_to_name(ret));
        return ret;
    }
    
    uint16_t scan_count = 0;
    ret = esp_wifi_scan_get_ap_num(&scan_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Network scan complete - Found %" PRIu32 " networks", (uint32_t)scan_count);
    
    return ESP_OK;
}