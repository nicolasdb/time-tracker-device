/**
 * @file payload_tool.c
 * @brief MCP-Inspired Payload Tool Implementation
 * 
 * Centralized payload formatting extracted from webhook_tool per process maps.
 * Implements proper timestamp coordination and device metadata management.
 */

#include "payload_tool.h"
#include "esp_event.h"      // ESP event system base
#include "esp_log.h"
#include "ntp_tool.h"       // NTP tool for sync checking per process map 13
#include "fs_tool.h"        // FS tool for payload storage
#include "http_tool.h"      // HTTP tool for payload transmission
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "payload_tool";

// =============================================================================
// RFID Event Declarations (from event_system.h - avoiding circular dependency)
// =============================================================================

ESP_EVENT_DECLARE_BASE(RFID_EVENTS);
ESP_EVENT_DECLARE_BASE(PAYLOAD_EVENTS);

typedef enum {
    RFID_EVENT_READY = 0,
    RFID_EVENT_TAG_DETECTED = 1,
    RFID_EVENT_TAG_REMOVED = 2,
    RFID_EVENT_TAG_IGNORED = 3,
    RFID_EVENT_ERROR = 4
} rfid_event_id_t;

typedef enum {
    PAYLOAD_EVENT_READY = 0,
    PAYLOAD_EVENT_STORED = 1,
    PAYLOAD_EVENT_TRANSMITTED = 2,
    PAYLOAD_EVENT_FAILED = 3
} payload_event_id_t;

typedef struct {
    char tag_uid[20];
    uint64_t detection_time_us;
    uint32_t debounce_counter;
    bool is_valid;
} rfid_event_data_t;


// =============================================================================
// Tool Context Structure
// =============================================================================

typedef struct payload_tool {
    payload_tool_config_t config;
    bool initialized;
    uint32_t payloads_formatted;
    uint32_t boot_counter;
    bool ntp_integration_active;
    uint32_t last_error_code;
    nvs_handle_t nvs_handle;
    
    // Event system integration per process map 13
    esp_event_handler_instance_t rfid_event_handler;
    bool event_subscription_active;
    
    // Tool dependencies for process map compliance
    void* fs_tool_handle;      // FS tool for payload storage
    void* ntp_tool_handle;     // NTP tool for timestamp coordination
    void* http_tool_handle;    // HTTP tool for transmission
} payload_tool_t;

// =============================================================================
// NVS Keys for Persistence
// =============================================================================

#define PAYLOAD_NVS_NAMESPACE "payload_tool"
#define PAYLOAD_NVS_BOOT_COUNT "boot_count"

// =============================================================================
// Forward Declarations
// =============================================================================

static void payload_tool_rfid_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t payload_tool_process_rfid_event(payload_tool_handle_t handle, rfid_event_id_t event_id, rfid_event_data_t* event_data);

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

payload_tool_config_t payload_tool_create_default_config(void)
{
    payload_tool_config_t config = {
        .device_id = {0},
        .enable_boot_counter = true,
        .enable_ntp_integration = true,
        .timestamp_precision_us = 1000  // 1ms precision
    };
    
    // Generate device ID from MAC address
    payload_tool_generate_device_id(config.device_id);
    
    return config;
}

payload_tool_handle_t payload_tool_init(const payload_tool_config_t* config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    payload_tool_t* tool = calloc(1, sizeof(payload_tool_t));
    if (!tool) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }
    
    // Copy configuration
    memcpy(&tool->config, config, sizeof(payload_tool_config_t));
    
    // Initialize NVS for boot counter persistence
    if (tool->config.enable_boot_counter) {
        esp_err_t ret = nvs_open(PAYLOAD_NVS_NAMESPACE, NVS_READWRITE, &tool->nvs_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
            tool->config.enable_boot_counter = false;
        } else {
            // Load existing boot counter
            size_t required_size = sizeof(uint32_t);
            ret = nvs_get_blob(tool->nvs_handle, PAYLOAD_NVS_BOOT_COUNT, 
                              &tool->boot_counter, &required_size);
            if (ret == ESP_ERR_NVS_NOT_FOUND) {
                tool->boot_counter = 0;
                ESP_LOGI(TAG, "Boot counter initialized to 0");
            } else if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to read boot counter: %s", esp_err_to_name(ret));
                tool->boot_counter = 0;
            } else {
                ESP_LOGI(TAG, "Boot counter loaded: %lu", tool->boot_counter);
            }
        }
    }
    
    tool->initialized = true;
    tool->ntp_integration_active = tool->config.enable_ntp_integration;
    tool->event_subscription_active = false;
    
    // Initialize tool dependency handles to NULL
    tool->fs_tool_handle = NULL;
    tool->ntp_tool_handle = NULL;
    tool->http_tool_handle = NULL;
    
    ESP_LOGI(TAG, "Payload tool initialized - Device ID: %s", tool->config.device_id);
    
    return tool;
}

esp_err_t payload_tool_cleanup(payload_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    // Unregister RFID event handler if subscribed
    if (tool->event_subscription_active && tool->rfid_event_handler) {
        esp_err_t unregister_ret = esp_event_handler_instance_unregister(RFID_EVENTS, ESP_EVENT_ANY_ID, tool->rfid_event_handler);
        if (unregister_ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ RFID event subscription deregistered");
        } else {
            ESP_LOGW(TAG, "⚠️ Failed to deregister RFID event subscription: %s", esp_err_to_name(unregister_ret));
        }
        tool->event_subscription_active = false;
    }
    
    // Close NVS handle
    if (tool->config.enable_boot_counter && tool->nvs_handle) {
        nvs_close(tool->nvs_handle);
    }
    
    free(tool);
    
    ESP_LOGI(TAG, "Payload tool cleaned up");
    
    return ESP_OK;
}

const char* payload_tool_get_id(void)
{
    return PAYLOAD_TOOL_ID;
}

const char* payload_tool_get_version(void)
{
    return PAYLOAD_TOOL_VERSION;
}

payload_tool_capabilities_t payload_tool_get_capabilities(payload_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    payload_tool_capabilities_t caps = PAYLOAD_CAP_JSON_FORMATTING | 
                                     PAYLOAD_CAP_TIMESTAMP_CALC |
                                     PAYLOAD_CAP_DEVICE_METADATA |
                                     PAYLOAD_CAP_THREAD_SAFE;
    
    if (tool->config.enable_ntp_integration) {
        caps |= PAYLOAD_CAP_NTP_INTEGRATION;
    }
    
    if (tool->config.enable_boot_counter) {
        caps |= PAYLOAD_CAP_BOOT_COUNTER;
    }
    
    return caps;
}

esp_err_t payload_tool_get_status(payload_tool_handle_t handle, payload_tool_status_t* status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    status->is_initialized = tool->initialized;
    status->payloads_formatted = tool->payloads_formatted;
    status->boot_counter_value = tool->boot_counter;
    status->ntp_integration_active = tool->ntp_integration_active;
    status->last_error_code = tool->last_error_code;
    
    return ESP_OK;
}

// =============================================================================
// Core Payload Formatting Functions
// =============================================================================

static const char* payload_event_type_to_string(payload_event_type_t event_type)
{
    switch (event_type) {
        case PAYLOAD_EVENT_TAG_PLACED:
            return "tag_placed";
        case PAYLOAD_EVENT_TAG_REMOVED:
            return "tag_removed";
        case PAYLOAD_EVENT_SYSTEM_BOOT:
            return "system_boot";
        case PAYLOAD_EVENT_SYSTEM_ERROR:
            return "system_error";
        default:
            return "unknown";
    }
}

esp_err_t payload_tool_format_event(payload_tool_handle_t handle,
                                  const payload_event_data_t* event_data,
                                  payload_formatted_result_t* result)
{
    if (!handle || !event_data || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    // Initialize result
    memset(result, 0, sizeof(payload_formatted_result_t));
    
    // Calculate timestamp with NTP coordination
    int64_t ntp_offset_ms = 0;
    esp_err_t ts_ret = payload_tool_calculate_timestamp(handle,
                                                       event_data->internal_timestamp_us,
                                                       result->iso_timestamp,
                                                       &ntp_offset_ms);
    if (ts_ret != ESP_OK) {
        ESP_LOGW(TAG, "Timestamp calculation failed: %s", esp_err_to_name(ts_ret));
        tool->last_error_code = ts_ret;
    }
    
    // Create JSON payload (following process map format)
    cJSON* json = cJSON_CreateObject();
    if (!json) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        tool->last_error_code = ESP_ERR_NO_MEM;
        return ESP_ERR_NO_MEM;
    }
    
    // Add required fields per process map authority
    cJSON_AddStringToObject(json, "device_id", tool->config.device_id);
    cJSON_AddStringToObject(json, "event_type", payload_event_type_to_string(event_data->event_type));
    cJSON_AddStringToObject(json, "timestamp", result->iso_timestamp);
    
    // Add tag UID if present
    if (strlen(event_data->tag_uid) > 0) {
        cJSON_AddStringToObject(json, "tag_uid", event_data->tag_uid);
    }
    
    // Add metadata object
    cJSON* metadata = cJSON_CreateObject();
    if (metadata) {
        // Internal timestamp in milliseconds
        uint64_t internal_millis = event_data->internal_timestamp_us / 1000;
        cJSON_AddNumberToObject(metadata, "internal_millis", (double)internal_millis);
        
        // NTP offset
        cJSON_AddNumberToObject(metadata, "ntp_offset_ms", (double)ntp_offset_ms);
        cJSON_AddBoolToObject(metadata, "ntp_synced", event_data->ntp_synced);
        
        // Boot counter
        cJSON_AddNumberToObject(metadata, "boot_counter", (double)tool->boot_counter);
        
        // Add metadata to main object
        cJSON_AddItemToObject(json, "metadata", metadata);
    }
    
    // Add additional data if present
    if (strlen(event_data->additional_data) > 0) {
        cJSON* additional = cJSON_Parse(event_data->additional_data);
        if (additional) {
            cJSON_AddItemToObject(json, "additional", additional);
        } else {
            cJSON_AddStringToObject(json, "additional_data", event_data->additional_data);
        }
    }
    
    // Convert to string
    result->json_string = cJSON_Print(json);
    cJSON_Delete(json);
    
    if (!result->json_string) {
        ESP_LOGE(TAG, "Failed to convert JSON to string");
        tool->last_error_code = ESP_ERR_NO_MEM;
        return ESP_ERR_NO_MEM;
    }
    
    result->json_length = strlen(result->json_string);
    result->formatting_success = true;
    
    // Update statistics
    tool->payloads_formatted++;
    
    ESP_LOGD(TAG, "Formatted payload (%zu bytes): %s", result->json_length, result->json_string);
    
    return ESP_OK;
}

void payload_tool_free_result(payload_formatted_result_t* result)
{
    if (result && result->json_string) {
        free(result->json_string);
        result->json_string = NULL;
        result->json_length = 0;
        result->formatting_success = false;
    }
}

esp_err_t payload_tool_calculate_timestamp(payload_tool_handle_t handle,
                                         uint64_t internal_timestamp_us,
                                         char* iso_timestamp,
                                         int64_t* ntp_offset_ms)
{
    if (!handle || !iso_timestamp || !ntp_offset_ms) {
        return ESP_ERR_INVALID_ARG;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    // Process Map 13: Calculate real timestamp = event_uptime + ntp_offset
    *ntp_offset_ms = 0;
    
    // Get NTP offset if NTP tool is available and synced
    if (tool->ntp_tool_handle) {
        ntp_tool_status_t ntp_status;
        esp_err_t ntp_ret = ntp_tool_get_status((ntp_tool_handle_t)tool->ntp_tool_handle, &ntp_status);
        if (ntp_ret == ESP_OK && ntp_status.sync_status == NTP_STATUS_SYNCED) {
            // Calculate proper NTP offset: real_timestamp = event_uptime + ntp_offset
            *ntp_offset_ms = ntp_status.last_offset_us / 1000;  // Convert μs to ms
            ESP_LOGD(TAG, "🕐 Using NTP offset: %lld ms (last sync: %lld)", 
                     (long long)*ntp_offset_ms, (long long)ntp_status.last_sync_time);
        } else {
            ESP_LOGW(TAG, "⚠️ NTP not synced - using system time");
        }
    }
    
    // Calculate final timestamp
    uint64_t final_timestamp_ms = (internal_timestamp_us / 1000) + *ntp_offset_ms;
    time_t utc_time = final_timestamp_ms / 1000;
    uint32_t millisecond_part = final_timestamp_ms % 1000;
    
    struct tm* utc_tm = gmtime(&utc_time);
    if (!utc_tm) {
        ESP_LOGE(TAG, "❌ Failed to convert time to UTC");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Format as ISO 8601 with millisecond precision per process map authority
    int ret = snprintf(iso_timestamp, 32, "%04d-%02d-%02dT%02d:%02d:%02d.%03luZ",
                      utc_tm->tm_year + 1900,
                      utc_tm->tm_mon + 1,
                      utc_tm->tm_mday,
                      utc_tm->tm_hour,
                      utc_tm->tm_min,
                      utc_tm->tm_sec,
                      (unsigned long)millisecond_part);
    
    if (ret < 0 || ret >= 32) {
        ESP_LOGE(TAG, "❌ Failed to format ISO timestamp");
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGD(TAG, "🕐 Calculated timestamp: %s (offset: %lld ms)", 
             iso_timestamp, (long long)*ntp_offset_ms);
    
    return ESP_OK;
}

esp_err_t payload_tool_get_boot_counter(payload_tool_handle_t handle, uint32_t* boot_counter)
{
    if (!handle || !boot_counter) {
        return ESP_ERR_INVALID_ARG;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    *boot_counter = tool->boot_counter;
    
    return ESP_OK;
}

esp_err_t payload_tool_increment_boot_counter(payload_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    if (!tool->config.enable_boot_counter) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    tool->boot_counter++;
    
    // Save to NVS
    esp_err_t ret = nvs_set_blob(tool->nvs_handle, PAYLOAD_NVS_BOOT_COUNT,
                                &tool->boot_counter, sizeof(uint32_t));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save boot counter: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_commit(tool->nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to commit boot counter: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Boot counter incremented to %lu", tool->boot_counter);
    
    return ESP_OK;
}

esp_err_t payload_tool_generate_device_id(char* device_id)
{
    if (!device_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t mac[6];
    esp_err_t ret = esp_efuse_mac_get_default(mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Format as ESP32_XXXXXX using last 3 bytes of MAC
    int snprintf_ret = snprintf(device_id, 32, "ESP32_%02X%02X%02X",
                               mac[3], mac[4], mac[5]);
    
    if (snprintf_ret < 0 || snprintf_ret >= 32) {
        ESP_LOGE(TAG, "Failed to format device ID");
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGD(TAG, "Generated device ID: %s", device_id);
    
    return ESP_OK;
}

// =============================================================================
// Event System Integration (Process Map 13 Compliance)
// =============================================================================

esp_err_t payload_tool_start_event_subscription(payload_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    if (tool->event_subscription_active) {
        ESP_LOGW(TAG, "RFID event subscription already active");
        return ESP_OK;
    }
    
    // Register for ALL RFID events per process map authority
    esp_err_t ret = esp_event_handler_instance_register(
        RFID_EVENTS, 
        ESP_EVENT_ANY_ID,
        payload_tool_rfid_event_handler,
        tool,
        &tool->rfid_event_handler
    );
    
    if (ret == ESP_OK) {
        tool->event_subscription_active = true;
        ESP_LOGI(TAG, "✅ RFID event subscription started - Process Map 13 active");
        ESP_LOGI(TAG, "📦 Payload tool will receive: TAG_DETECTED, TAG_REMOVED events");
    } else {
        ESP_LOGE(TAG, "❌ Failed to register RFID event handler: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t payload_tool_set_dependencies(payload_tool_handle_t handle, 
                                       void* fs_tool_handle,
                                       void* ntp_tool_handle, 
                                       void* http_tool_handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    tool->fs_tool_handle = fs_tool_handle;
    tool->ntp_tool_handle = ntp_tool_handle;
    tool->http_tool_handle = http_tool_handle;
    
    ESP_LOGI(TAG, "🔗 Tool dependencies set: FS=%p, NTP=%p, HTTP=%p", 
             fs_tool_handle, ntp_tool_handle, http_tool_handle);
    
    return ESP_OK;
}

// =============================================================================
// RFID Event Handler Implementation (Process Map 13)
// =============================================================================

static void payload_tool_rfid_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    payload_tool_t* tool = (payload_tool_t*)arg;
    
    if (event_base != RFID_EVENTS || !tool || !event_data) {
        ESP_LOGW(TAG, "❌ Invalid RFID event parameters");
        return;
    }
    
    ESP_LOGI(TAG, "📡 RFID event received: event_id=%ld", event_id);
    
    rfid_event_data_t* rfid_data = (rfid_event_data_t*)event_data;
    
    // Process event per process map 13
    esp_err_t process_ret = payload_tool_process_rfid_event(tool, (rfid_event_id_t)event_id, rfid_data);
    if (process_ret != ESP_OK) {
        ESP_LOGW(TAG, "❌ Failed to process RFID event: %s", esp_err_to_name(process_ret));
        tool->last_error_code = process_ret;
    }
}

static esp_err_t payload_tool_process_rfid_event(payload_tool_handle_t handle, rfid_event_id_t event_id, rfid_event_data_t* event_data)
{
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    // Process Map 13: IDLE → S1 (event received) → BUILD_PAYLOAD or BACKLOG (wait for NTP_SYNC)
    
    switch (event_id) {
        case RFID_EVENT_TAG_DETECTED:
        case RFID_EVENT_TAG_REMOVED:
            ESP_LOGI(TAG, "🏷️ Processing %s event: tag=%s", 
                     event_id == RFID_EVENT_TAG_DETECTED ? "TAG_DETECTED" : "TAG_REMOVED",
                     event_data->tag_uid);
            
            // Check NTP sync status per process map 13 (S1 choice)
            bool ntp_synced = false;
            
            if (tool->ntp_tool_handle) {
                ntp_tool_status_t ntp_status;
                esp_err_t ntp_ret = ntp_tool_get_status((ntp_tool_handle_t)tool->ntp_tool_handle, &ntp_status);
                if (ntp_ret == ESP_OK) {
                    ntp_synced = (ntp_status.sync_status == NTP_STATUS_SYNCED);
                    ESP_LOGD(TAG, "🕐 NTP sync status: %s (last sync: %lld)", 
                             ntp_synced ? "synced" : "not synced", (long long)ntp_status.last_sync_time);
                } else {
                    ESP_LOGW(TAG, "⚠️ Failed to get NTP status: %s", esp_err_to_name(ntp_ret));
                }
            } else {
                ESP_LOGW(TAG, "⚠️ NTP tool not available - proceeding without sync check");
                ntp_synced = true; // Allow processing when NTP tool is not available
            }
            
            if (!ntp_synced) {
                ESP_LOGW(TAG, "⏳ NTP not synced - event queued for later (BACKLOG state per Process Map 13)");
                // TODO: Implement backlog queue per process map 13
                return ESP_OK; // Not an error, just deferred
            }
            
            // Build payload per process map 13: BUILD_PAYLOAD → CALC_TIMESTAMP → ADD_DEVICE_INFO → ADD_HEALTH
            return payload_tool_build_and_transmit_payload(tool, event_id, event_data);
            
        case RFID_EVENT_TAG_IGNORED:
            ESP_LOGD(TAG, "🏷️ Tag ignored event - no payload processing needed");
            return ESP_OK;
            
        default:
            ESP_LOGW(TAG, "❓ Unhandled RFID event in payload tool: event_id=%d", event_id);
            return ESP_OK;
    }
}

esp_err_t payload_tool_build_and_transmit_payload(payload_tool_handle_t handle, int event_id, void* event_data)
{
    payload_tool_t* tool = (payload_tool_t*)handle;
    
    if (!tool || !event_data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "📦 Building payload per Process Map 13 authority");
    
    // Cast event_data back to rfid_event_data_t
    rfid_event_data_t* rfid_data = (rfid_event_data_t*)event_data;
    
    // Create payload event data from RFID event
    payload_event_data_t payload_data = {
        .event_type = (event_id == RFID_EVENT_TAG_DETECTED) ? PAYLOAD_EVENT_TAG_PLACED : PAYLOAD_EVENT_TAG_REMOVED,
        .internal_timestamp_us = rfid_data->detection_time_us,
        .ntp_synced = true,  // Already checked in caller
        .boot_counter = tool->boot_counter
    };
    
    // Copy tag UID
    strncpy(payload_data.tag_uid, rfid_data->tag_uid, sizeof(payload_data.tag_uid) - 1);
    payload_data.tag_uid[sizeof(payload_data.tag_uid) - 1] = '\0';
    
    // Format payload using existing function
    payload_formatted_result_t result;
    esp_err_t format_ret = payload_tool_format_event(tool, &payload_data, &result);
    
    if (format_ret != ESP_OK || !result.formatting_success) {
        ESP_LOGE(TAG, "❌ Payload formatting failed: %s", esp_err_to_name(format_ret));
        return format_ret;
    }
    
    ESP_LOGI(TAG, "✅ Payload formatted successfully (%zu bytes)", result.json_length);
    
    // Process Map 13 Constitutional Authority: PAYLOAD_READY → esp_event_post
    // This triggers both STORE_PAYLOAD and SEND_PAYLOAD via event subscriptions
    
    ESP_LOGI(TAG, "📡 Publishing PAYLOAD_READY event per Process Map 13 authority");
    
    // Create payload event data for ESP event system
    payload_event_data_t* event_payload = malloc(sizeof(payload_event_data_t) + result.json_length + 1);
    if (!event_payload) {
        ESP_LOGE(TAG, "❌ Failed to allocate memory for payload event");
        payload_tool_free_result(&result);
        return ESP_ERR_NO_MEM;
    }
    
    // Copy payload data and JSON string
    memcpy(event_payload, &payload_data, sizeof(payload_event_data_t));
    strcpy((char*)(event_payload + 1), result.json_string);
    
    // Constitutional Authority: Pure event-driven communication
    esp_err_t event_ret = esp_event_post(PAYLOAD_EVENTS, PAYLOAD_EVENT_READY, 
                                        event_payload, 
                                        sizeof(payload_event_data_t) + result.json_length + 1,
                                        100 / portTICK_PERIOD_MS);
    
    if (event_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ PAYLOAD_READY event published - fs_tool and http_tool will handle");
    } else {
        ESP_LOGE(TAG, "❌ Failed to publish PAYLOAD_READY event: %s", esp_err_to_name(event_ret));
    }
    
    free(event_payload);
    
    // Update statistics
    tool->payloads_formatted++;
    
    // Clean up
    payload_tool_free_result(&result);
    
    // Constitutional Authority: Event published - tools will handle asynchronously
    return event_ret;
}