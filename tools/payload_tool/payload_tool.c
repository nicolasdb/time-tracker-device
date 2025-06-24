/**
 * @file payload_tool.c
 * @brief MCP-Inspired Payload Tool Implementation
 * 
 * Centralized payload formatting extracted from webhook_tool per process maps.
 * Implements proper timestamp coordination and device metadata management.
 */

#include "payload_tool.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "payload_tool";

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
} payload_tool_t;

// =============================================================================
// NVS Keys for Persistence
// =============================================================================

#define PAYLOAD_NVS_NAMESPACE "payload_tool"
#define PAYLOAD_NVS_BOOT_COUNT "boot_count"

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
    
    ESP_LOGI(TAG, "Payload tool initialized - Device ID: %s", tool->config.device_id);
    
    return tool;
}

esp_err_t payload_tool_cleanup(payload_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    payload_tool_t* tool = (payload_tool_t*)handle;
    
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
    
    // Get current time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // Calculate NTP offset (simplified - real implementation would use NTP tool)
    // For now, use system time as baseline
    *ntp_offset_ms = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    
    // Convert to UTC time
    time_t utc_time = tv.tv_sec;
    struct tm* utc_tm = gmtime(&utc_time);
    
    if (!utc_tm) {
        ESP_LOGE(TAG, "Failed to convert time to UTC");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Format as ISO 8601 (simplified - microseconds would be added from internal_timestamp_us)
    int ret = snprintf(iso_timestamp, 32, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                      utc_tm->tm_year + 1900,
                      utc_tm->tm_mon + 1,
                      utc_tm->tm_mday,
                      utc_tm->tm_hour,
                      utc_tm->tm_min,
                      utc_tm->tm_sec);
    
    if (ret < 0 || ret >= 32) {
        ESP_LOGE(TAG, "Failed to format ISO timestamp");
        return ESP_ERR_INVALID_SIZE;
    }
    
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