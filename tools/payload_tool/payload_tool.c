/**
 * @file payload_tool.c
 * @brief Constitutional Payload Tool Implementation - JSON Data Formatting
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Process Map 13 (payload_fsm.mmd)
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#include "payload_tool.h"
#include "fs_tool.h"
#include "ntp_tool.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

static const char* TAG = "payload_tool";

// Constitutional Payload Tool Event Base
ESP_EVENT_DEFINE_BASE(PAYLOAD_TOOL_EVENTS);

// Constitutional Tool Context (Handle-based pattern)
struct payload_tool {
    bool is_initialized;
    bool is_active;
    payload_tool_config_t config;
    payload_tool_status_t status;
    uint64_t init_timestamp_us;
    uint32_t payloads_created;
    uint32_t validation_errors;
    uint32_t format_errors;
    
    // Current device metadata cache
    payload_device_metadata_t cached_device_metadata;
    uint64_t metadata_cache_timestamp_us;
    uint32_t metadata_cache_ttl_ms;
    
    // Constitutional tool dependencies
    void* ntp_tool;
    fs_tool_handle_t fs_tool;
    
    // Thread safety
    SemaphoreHandle_t state_mutex;
    esp_event_loop_handle_t event_loop;
};

// =============================================================================
// Constitutional Timestamp Functions  
// =============================================================================

/**
 * @brief Calculate real Unix timestamp from esp_timer using NTP correlation
 * Implements Issue #6 correct timestamp calculation approach
 * Formula: real_timestamp = real_time_at_sync + (current_esp_timer - esp_timer_at_sync)
 */
static uint64_t constitutional_calculate_real_timestamp_us(payload_tool_handle_t handle, uint64_t esp_timer_us)
{
    if (!handle || !handle->ntp_tool) {
        ESP_LOGW(TAG, "⚠️ No NTP tool available - using esp_timer: %" PRIu64, esp_timer_us);
        return esp_timer_us;
    }
    
    // Get NTP sync correlation data
    time_t real_time_at_sync = 0;
    uint64_t esp_timer_at_sync = 0;
    
    // Call ntp_tool_get_sync_correlation to get correlation data
    esp_err_t ret = ntp_tool_get_sync_correlation((ntp_tool_handle_t)handle->ntp_tool, 
                                                  &real_time_at_sync, 
                                                  &esp_timer_at_sync);
    
    if (ret == ESP_OK) {
        // Calculate real timestamp using NTP correlation
        // Formula: real_timestamp = real_time_at_sync + (current_esp_timer - esp_timer_at_sync)
        uint64_t time_elapsed_us = esp_timer_us - esp_timer_at_sync;
        uint64_t real_timestamp_us = (uint64_t)real_time_at_sync * 1000000ULL + time_elapsed_us;
        
        ESP_LOGI(TAG, "✅ Calculated real timestamp using NTP correlation: %" PRIu64 
                 " (sync_time=%" PRIu32 ", elapsed=%" PRIu64 ")", 
                 real_timestamp_us, (uint32_t)real_time_at_sync, time_elapsed_us);
        
        return real_timestamp_us;
    }
    
    // Fallback: try system time if NTP correlation not available
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        uint64_t current_unix_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
        
        // Validate timestamp is reasonable (after 2024-01-01)
        if (current_unix_us > 1704067200000000ULL) {
            ESP_LOGI(TAG, "✅ Using fallback system time: %" PRIu64, current_unix_us);
            return current_unix_us;
        }
    }
    
    // Final fallback to esp_timer if nothing else works
    ESP_LOGW(TAG, "⚠️ No valid time source - using esp_timer: %" PRIu64, esp_timer_us);
    return esp_timer_us;
}

// Note: Timezone conversion is now handled in the NTP tool's timezone setup
// This ensures single point of timezone application via setenv()/tzset()

// =============================================================================
// Constitutional Device Metadata Functions
// =============================================================================

/**
 * @brief Constitutional device metadata collection
 * Gathers system information with constitutional memory safety
 */
static esp_err_t constitutional_collect_device_metadata(payload_tool_handle_t handle,
                                                       payload_device_metadata_t *metadata)
{
    if (!handle || !metadata) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Generate device ID from MAC address
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret == ESP_OK) {
        snprintf(metadata->device_id, sizeof(metadata->device_id),
                "%02X%02X%02X%02X%02X%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        snprintf(metadata->device_id, sizeof(metadata->device_id), "UNKNOWN");
    }
    
    // Firmware version
    snprintf(metadata->firmware_version, sizeof(metadata->firmware_version), "%s", "6.1.0");
    
    // Hardware revision
    snprintf(metadata->hardware_revision, sizeof(metadata->hardware_revision), "ESP32-C3-v1.0");
    
    // System uptime
    metadata->uptime_us = esp_timer_get_time();
    
    // Free memory
    metadata->free_memory_bytes = esp_get_free_heap_size();
    
    // WiFi RSSI (placeholder - would need network tool integration)
    metadata->wifi_rssi = -50; // Default reasonable value
    
    return ESP_OK;
}

/**
 * @brief Constitutional payload validation
 * Validates payload structure and content
 */
static esp_err_t constitutional_validate_payload_structure(payload_tool_handle_t handle,
                                                          const payload_data_t *payload)
{
    if (!handle || !payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate session data
    if (strlen(payload->session.rfid_uid) == 0) {
        ESP_LOGW(TAG, "⚠️ Payload validation: Missing RFID UID");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (payload->session.timestamp_us == 0) {
        ESP_LOGW(TAG, "⚠️ Payload validation: Invalid timestamp");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate device metadata
    if (strlen(payload->device.device_id) == 0) {
        ESP_LOGW(TAG, "⚠️ Payload validation: Missing device ID");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate JSON payload
    if (payload->json_size == 0 || strlen(payload->json_payload) == 0) {
        ESP_LOGW(TAG, "⚠️ Payload validation: Empty JSON payload");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate JSON structure
    cJSON *json = cJSON_Parse(payload->json_payload);
    if (!json) {
        ESP_LOGW(TAG, "⚠️ Payload validation: Invalid JSON structure");
        return ESP_ERR_INVALID_STATE;
    }
    
    cJSON_Delete(json);
    
    ESP_LOGI(TAG, "✅ Payload validation successful");
    return ESP_OK;
}

// =============================================================================
// Constitutional Tool Interface Implementation
// =============================================================================

const char* payload_tool_get_id(void)
{
    return PAYLOAD_TOOL_ID;
}

const char* payload_tool_get_version(void)
{
    return PAYLOAD_TOOL_VERSION;
}

payload_tool_config_t payload_tool_create_default_config(void)
{
    payload_tool_config_t config = {0};
    
    // Output formatting
    config.include_device_metadata = true;
    config.include_debug_info = false;
    config.compress_payload = false;
    config.encrypt_payload = false;
    
    // Session tracking
    config.enable_session_tracking = true;
    config.min_session_duration_ms = 5000;   // 5 seconds
    config.max_session_duration_ms = 28800000; // 8 hours
    
    // Validation settings
    config.enable_payload_validation = true;
    config.require_ntp_sync = true;
    
    // Event publishing
    config.publish_events = true;
    config.event_queue_size = 8;
    
    // Server integration settings (use Kconfig as source of truth)
#ifdef CONFIG_PAYLOAD_USE_SERVER_FORMAT
    config.use_server_format = CONFIG_PAYLOAD_USE_SERVER_FORMAT;
#else
    config.use_server_format = true; // Default to server format for compatibility
#endif

#ifdef CONFIG_PAYLOAD_MINIMAL_MODE
    config.minimal_mode = CONFIG_PAYLOAD_MINIMAL_MODE;
#else
    config.minimal_mode = false;
#endif

#ifdef CONFIG_PAYLOAD_INCLUDE_DIAGNOSTICS
    config.include_diagnostics = CONFIG_PAYLOAD_INCLUDE_DIAGNOSTICS;
#else
    config.include_diagnostics = true;
#endif

#ifdef CONFIG_PAYLOAD_INCLUDE_NETWORK_INFO
    config.include_network_info = CONFIG_PAYLOAD_INCLUDE_NETWORK_INFO;
#else
    config.include_network_info = true;
#endif

#ifdef CONFIG_PAYLOAD_EVENT_CONFIDENCE_THRESHOLD
    config.event_confidence_threshold = CONFIG_PAYLOAD_EVENT_CONFIDENCE_THRESHOLD;
#else
    config.event_confidence_threshold = 95;
#endif
    
    return config;
}

payload_tool_handle_t payload_tool_init(const payload_tool_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return NULL;
    }
    
    ESP_LOGI(TAG, "🏗️ Constitutional payload tool initializing");
    
    // Allocate handle with constitutional memory safety
    payload_tool_handle_t handle = malloc(sizeof(struct payload_tool));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate payload tool handle");
        return NULL;
    }
    
    // Initialize handle with constitutional patterns
    memset(handle, 0, sizeof(struct payload_tool));
    memcpy(&handle->config, config, sizeof(payload_tool_config_t));
    handle->init_timestamp_us = esp_timer_get_time();
    handle->metadata_cache_ttl_ms = 30000; // 30 second cache TTL
    
    // Create state mutex
    handle->state_mutex = xSemaphoreCreateMutex();
    if (!handle->state_mutex) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        free(handle);
        return NULL;
    }
    
    // Initialize device metadata cache
    constitutional_collect_device_metadata(handle, &handle->cached_device_metadata);
    handle->metadata_cache_timestamp_us = esp_timer_get_time();
    
    // Initialize status
    handle->status.is_initialized = true;
    handle->status.is_active = true;
    handle->status.ntp_sync_required = config->require_ntp_sync;
    handle->status.validation_enabled = config->enable_payload_validation;
    handle->status.capabilities = PAYLOAD_CAP_JSON_FORMAT | PAYLOAD_CAP_SESSION_TRACKING |
                                 PAYLOAD_CAP_DEVICE_METADATA | PAYLOAD_CAP_VALIDATION;
    
    // Set control flags
    handle->is_initialized = true;
    handle->is_active = true;
    
    ESP_LOGI(TAG, "✅ Constitutional payload tool: %s v%s initialized", 
             payload_tool_get_id(), payload_tool_get_version());
    
    return handle;
}

esp_err_t payload_tool_deinit(payload_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔌 Constitutional payload tool deinitializing");
    
    // Stop operations
    handle->is_active = false;
    
    // Clean up resources
    if (handle->state_mutex) {
        vSemaphoreDelete(handle->state_mutex);
    }
    
    // Constitutional cleanup
    free(handle);
    
    ESP_LOGI(TAG, "✅ Constitutional payload tool deinitialized");
    
    return ESP_OK;
}

payload_tool_capabilities_t payload_tool_get_capabilities(payload_tool_handle_t handle)
{
    if (!handle) {
        return 0;
    }
    
    return handle->status.capabilities;
}

esp_err_t payload_tool_get_status(payload_tool_handle_t handle, payload_tool_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(status, &handle->status, sizeof(payload_tool_status_t));
        status->uptime_us = esp_timer_get_time() - handle->init_timestamp_us;
        status->payloads_created = handle->payloads_created;
        status->validation_errors = handle->validation_errors;
        status->format_errors = handle->format_errors;
        xSemaphoreGive(handle->state_mutex);
    }
    
    return ESP_OK;
}

esp_err_t payload_tool_set_ntp_dependency(payload_tool_handle_t handle, void* ntp_tool)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->ntp_tool = ntp_tool;
    ESP_LOGI(TAG, "✅ NTP tool dependency set for timestamp generation");
    
    return ESP_OK;
}

esp_err_t payload_tool_set_fs_dependency(payload_tool_handle_t handle, void* fs_tool)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->fs_tool = (fs_tool_handle_t)fs_tool;
    ESP_LOGI(TAG, "✅ FS tool dependency set for payload storage");
    
    return ESP_OK;
}

// =============================================================================
// Constitutional Payload Operations Implementation
// =============================================================================

esp_err_t payload_tool_create_session_payload(payload_tool_handle_t handle, 
                                             const payload_work_session_t *session,
                                             payload_data_t *payload)
{
    if (!handle || !session || !payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "📦 Creating constitutional session payload");
    
    if (xSemaphoreTake(handle->state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Clear payload structure
        memset(payload, 0, sizeof(payload_data_t));
        
        // Copy session data
        memcpy(&payload->session, session, sizeof(payload_work_session_t));
        
        // Update device metadata cache if needed
        uint64_t current_time = esp_timer_get_time();
        if ((current_time - handle->metadata_cache_timestamp_us) > (handle->metadata_cache_ttl_ms * 1000)) {
            constitutional_collect_device_metadata(handle, &handle->cached_device_metadata);
            handle->metadata_cache_timestamp_us = current_time;
        }
        
        // Copy device metadata
        memcpy(&payload->device, &handle->cached_device_metadata, sizeof(payload_device_metadata_t));
        
        // Calculate real Unix timestamp from session's esp_timer timestamp
        payload->creation_timestamp_us = constitutional_calculate_real_timestamp_us(handle, payload->session.timestamp_us);
        
        // Format to JSON
        esp_err_t format_ret = payload_tool_format_to_json(handle, payload, 
                                                          payload->json_payload, 
                                                          sizeof(payload->json_payload));
        if (format_ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to format payload to JSON");
            handle->format_errors++;
            xSemaphoreGive(handle->state_mutex);
            return format_ret;
        }
        
        payload->json_size = strlen(payload->json_payload);
        
        // Validate payload if enabled
        if (handle->config.enable_payload_validation) {
            esp_err_t validate_ret = constitutional_validate_payload_structure(handle, payload);
            if (validate_ret != ESP_OK) {
                ESP_LOGE(TAG, "❌ Payload validation failed");
                handle->validation_errors++;
                payload->is_valid = false;
                xSemaphoreGive(handle->state_mutex);
                return validate_ret;
            }
        }
        
        // Calculate checksum
        payload->checksum = payload_tool_calculate_checksum(handle, payload);
        payload->is_valid = true;
        
        handle->payloads_created++;
        handle->status.payloads_created++;
        
        xSemaphoreGive(handle->state_mutex);
        
        ESP_LOGI(TAG, "✅ Session payload created successfully (size: %zu bytes)", payload->json_size);
        
        // Publish constitutional event
        payload_tool_event_t payload_event = {
            .type = PAYLOAD_TOOL_EVENT_CREATED,
            .payload = *payload,
            .timestamp_us = esp_timer_get_time(),
            .error_code = ESP_OK
        };
        
        esp_event_post(PAYLOAD_TOOL_EVENTS, PAYLOAD_TOOL_EVENT_CREATED, 
                      &payload_event, sizeof(payload_event), 0);
        
    } else {
        ESP_LOGE(TAG, "Failed to acquire state mutex for payload creation");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t payload_tool_format_to_json(payload_tool_handle_t handle,
                                     const payload_data_t *payload,
                                     char *json_buffer,
                                     size_t buffer_size)
{
    if (!handle || !payload || !json_buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create JSON structure with constitutional memory safety
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON root object");
        return ESP_ERR_NO_MEM;
    }
    
    // Check if we should use server format
    if (payload->use_server_format) {
        ESP_LOGI(TAG, "Using server-compatible format (rfid_poll_result)");
        
        // Create rfid_poll_result object
        cJSON *rfid_poll_result = cJSON_CreateObject();
        
        // Add core required fields
        cJSON_AddStringToObject(rfid_poll_result, "event_type", payload->rfid_poll_result.event_type);
        cJSON_AddStringToObject(rfid_poll_result, "tag_id", payload->rfid_poll_result.tag_id);
        cJSON_AddStringToObject(rfid_poll_result, "device_id", payload->rfid_poll_result.device_id);
        cJSON_AddStringToObject(rfid_poll_result, "timestamp", payload->rfid_poll_result.timestamp);
        cJSON_AddBoolToObject(rfid_poll_result, "tag_present", payload->rfid_poll_result.tag_present);
        
        // Add strategic supplemental fields
        cJSON_AddBoolToObject(rfid_poll_result, "ntp_synced", payload->rfid_poll_result.ntp_synced);
        cJSON_AddStringToObject(rfid_poll_result, "session_id", payload->rfid_poll_result.session_id);
        cJSON_AddNumberToObject(rfid_poll_result, "sequence_number", payload->rfid_poll_result.sequence_number);
        cJSON_AddNumberToObject(rfid_poll_result, "wifi_rssi", payload->rfid_poll_result.wifi_rssi);
        cJSON_AddStringToObject(rfid_poll_result, "wifi_status", payload->rfid_poll_result.wifi_status);
        cJSON_AddNumberToObject(rfid_poll_result, "free_memory_bytes", payload->rfid_poll_result.free_memory_bytes);
        cJSON_AddStringToObject(rfid_poll_result, "firmware_version", payload->rfid_poll_result.firmware_version);
        cJSON_AddNumberToObject(rfid_poll_result, "uptime_ms", payload->rfid_poll_result.uptime_ms);
        cJSON_AddNumberToObject(rfid_poll_result, "event_confidence", payload->rfid_poll_result.event_confidence);
        cJSON_AddNumberToObject(rfid_poll_result, "processing_time_ms", payload->rfid_poll_result.processing_time_ms);
        
        // Add to root object
        cJSON_AddItemToObject(root, "rfid_poll_result", rfid_poll_result);
        
    } else {
        ESP_LOGI(TAG, "Using legacy format (session/device/metadata)");
        
        // Legacy format - existing code
        // Add session information using pre-calculated timestamp
        cJSON *session = cJSON_CreateObject();
        cJSON_AddStringToObject(session, "type", payload_tool_session_type_to_string(payload->session.type));
        cJSON_AddStringToObject(session, "rfid_uid", payload->session.rfid_uid);
        
        // Use the pre-calculated creation timestamp (NTP correlation-based)
        uint64_t final_timestamp_us = payload->creation_timestamp_us;
        
        // Log human-readable timestamp for debugging (uses system timezone set by NTP tool)
        time_t readable_time = (time_t)(final_timestamp_us / 1000000ULL);
        struct tm *timeinfo = localtime(&readable_time);
        if (timeinfo) {
            ESP_LOGI(TAG, "🕐 Timestamp (NTP correlation): %04d-%02d-%02d %02d:%02d:%02d", 
                     timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                     timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        }
        
        cJSON_AddNumberToObject(session, "timestamp_us", (double)final_timestamp_us);
        
        cJSON_AddNumberToObject(session, "duration_ms", payload->session.session_duration_ms);
        cJSON_AddStringToObject(session, "project_id", payload->session.project_id);
        cJSON_AddStringToObject(session, "task_description", payload->session.task_description);
        cJSON_AddItemToObject(root, "session", session);
        
        // Add device metadata if enabled
        if (handle->config.include_device_metadata) {
            cJSON *device = cJSON_CreateObject();
            cJSON_AddStringToObject(device, "device_id", payload->device.device_id);
            cJSON_AddStringToObject(device, "firmware_version", payload->device.firmware_version);
            cJSON_AddStringToObject(device, "hardware_revision", payload->device.hardware_revision);
            cJSON_AddNumberToObject(device, "uptime_us", (double)payload->device.uptime_us);
            cJSON_AddNumberToObject(device, "free_memory_bytes", payload->device.free_memory_bytes);
            cJSON_AddNumberToObject(device, "wifi_rssi", payload->device.wifi_rssi);
            cJSON_AddItemToObject(root, "device", device);
        }
        
        // Add metadata using the same final timestamp (avoid duplicate conversion)
        cJSON *metadata = cJSON_CreateObject();
        cJSON_AddNumberToObject(metadata, "creation_timestamp_us", (double)final_timestamp_us);
        cJSON_AddNumberToObject(metadata, "checksum", payload->checksum);
        cJSON_AddBoolToObject(metadata, "is_valid", payload->is_valid);
        cJSON_AddItemToObject(root, "metadata", metadata);
    }
    
    // Convert to string
    char *json_string = cJSON_Print(root);
    if (!json_string) {
        ESP_LOGE(TAG, "Failed to convert JSON to string");
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    
    // Copy to output buffer with constitutional memory safety
    size_t json_len = strlen(json_string);
    if (json_len >= buffer_size) {
        ESP_LOGE(TAG, "JSON payload too large for buffer (%zu >= %zu)", json_len, buffer_size);
        free(json_string);
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    
    snprintf(json_buffer, buffer_size, "%s", json_string);
    
    // Cleanup
    free(json_string);
    cJSON_Delete(root);
    
    return ESP_OK;
}

esp_err_t payload_tool_validate_payload(payload_tool_handle_t handle,
                                       const payload_data_t *payload)
{
    if (!handle || !payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return constitutional_validate_payload_structure(handle, payload);
}

esp_err_t payload_tool_get_device_metadata(payload_tool_handle_t handle,
                                          payload_device_metadata_t *metadata)
{
    if (!handle || !metadata) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return constitutional_collect_device_metadata(handle, metadata);
}

uint32_t payload_tool_calculate_checksum(payload_tool_handle_t handle,
                                        const payload_data_t *payload)
{
    if (!handle || !payload) {
        return 0;
    }
    
    // Simple checksum calculation using CRC-like algorithm
    uint32_t checksum = 0xFFFFFFFF;
    const uint8_t *data = (const uint8_t*)&payload->session;
    size_t len = sizeof(payload_work_session_t) + sizeof(payload_device_metadata_t);
    
    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
        checksum = (checksum << 1) | (checksum >> 31);
    }
    
    return checksum;
}

esp_err_t payload_tool_hardware_self_test(payload_tool_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔧 Constitutional payload tool hardware self-test starting");
    
    // Test 1: JSON formatting capability
    ESP_LOGI(TAG, "  📋 Testing JSON formatting capability");
    payload_work_session_t test_session = {
        .type = PAYLOAD_SESSION_START,
        .timestamp_us = esp_timer_get_time(),
        .session_duration_ms = 0
    };
    snprintf(test_session.rfid_uid, sizeof(test_session.rfid_uid), "TEST1234");
    snprintf(test_session.project_id, sizeof(test_session.project_id), "TEST_PROJECT");
    snprintf(test_session.task_description, sizeof(test_session.task_description), "Self-test task");
    
    // Allocate payload on heap to avoid stack overflow
    payload_data_t *test_payload = malloc(sizeof(payload_data_t));
    if (!test_payload) {
        ESP_LOGE(TAG, "  ❌ Failed to allocate memory for test payload");
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = payload_tool_create_session_payload(handle, &test_session, test_payload);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  ✅ JSON formatting test successful");
    } else {
        ESP_LOGE(TAG, "  ❌ JSON formatting test failed");
        free(test_payload);
        return ret;
    }
    
    // Test 2: Validation capability
    ESP_LOGI(TAG, "  🔍 Testing payload validation capability");
    ret = payload_tool_validate_payload(handle, test_payload);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  ✅ Payload validation test successful");
    } else {
        ESP_LOGE(TAG, "  ❌ Payload validation test failed");
        free(test_payload);
        return ret;
    }
    
    // Test 3: Device metadata collection
    ESP_LOGI(TAG, "  📊 Testing device metadata collection");
    payload_device_metadata_t test_metadata;
    ret = payload_tool_get_device_metadata(handle, &test_metadata);
    if (ret == ESP_OK && strlen(test_metadata.device_id) > 0) {
        ESP_LOGI(TAG, "  ✅ Device metadata collection successful");
        ESP_LOGI(TAG, "    Device ID: %s", test_metadata.device_id);
    } else {
        ESP_LOGE(TAG, "  ❌ Device metadata collection failed");
        free(test_payload);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Cleanup heap allocation
    free(test_payload);
    
    ESP_LOGI(TAG, "✅ Constitutional payload tool hardware self-test complete");
    
    return ESP_OK;
}

esp_err_t payload_tool_create_batch_payload(payload_tool_handle_t handle,
                                           const payload_work_session_t *sessions,
                                           size_t session_count,
                                           payload_data_t *batch_payload)
{
    if (!handle || !sessions || session_count == 0 || !batch_payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "📦 Creating batch payload with %zu sessions", session_count);
    
    // For now, create a single payload from the first session
    // Full batch implementation would combine multiple sessions
    return payload_tool_create_session_payload(handle, &sessions[0], batch_payload);
}

// =============================================================================
// Constitutional Utility Functions Implementation
// =============================================================================

const char* payload_tool_event_to_string(payload_tool_event_type_t event_type)
{
    switch (event_type) {
        case PAYLOAD_TOOL_EVENT_CREATED:   return "CREATED";
        case PAYLOAD_TOOL_EVENT_FORMATTED: return "FORMATTED";
        case PAYLOAD_TOOL_EVENT_VALIDATED: return "VALIDATED";
        case PAYLOAD_TOOL_EVENT_ERROR:     return "ERROR";
        case PAYLOAD_TOOL_EVENT_READY:     return "READY";
        default:                           return "UNKNOWN";
    }
}

const char* payload_tool_session_type_to_string(payload_session_type_t session_type)
{
    switch (session_type) {
        case PAYLOAD_SESSION_START:   return "SESSION_START";
        case PAYLOAD_SESSION_END:     return "SESSION_END";
        case PAYLOAD_SESSION_ACTIVE:  return "SESSION_ACTIVE";
        case PAYLOAD_SESSION_UNKNOWN: return "SESSION_UNKNOWN";
        default:                      return "INVALID";
    }
}

const char* payload_tool_rfid_event_type_to_string(payload_rfid_event_type_t event_type)
{
    switch (event_type) {
        case PAYLOAD_RFID_TAG_INSERT:   return "tag_insert";
        case PAYLOAD_RFID_TAG_REMOVED:  return "tag_removed";
        case PAYLOAD_RFID_EVENT_UNKNOWN: return "unknown";
        default:                        return "invalid";
    }
}

esp_err_t payload_tool_create_rfid_payload(payload_tool_handle_t handle,
                                          payload_rfid_event_type_t event_type,
                                          const char *tag_id,
                                          bool tag_present,
                                          payload_data_t *payload)
{
    if (!handle || !tag_id || !payload) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Creating RFID payload for event type: %s, tag: %s", 
             payload_tool_rfid_event_type_to_string(event_type), tag_id);

    // Initialize payload structure
    memset(payload, 0, sizeof(payload_data_t));
    payload->use_server_format = handle->config.use_server_format;

    // Set core required fields
    snprintf(payload->rfid_poll_result.event_type, sizeof(payload->rfid_poll_result.event_type), 
             "%s", payload_tool_rfid_event_type_to_string(event_type));
    snprintf(payload->rfid_poll_result.tag_id, sizeof(payload->rfid_poll_result.tag_id), 
             "%s", tag_id);
    payload->rfid_poll_result.tag_present = tag_present;

    // Generate device ID
    esp_err_t ret = payload_tool_generate_device_id(payload->rfid_poll_result.device_id, 
                                                   sizeof(payload->rfid_poll_result.device_id));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate device ID");
        return ret;
    }

    // Get current time and format as ISO 8601 (local time using NTP tool timezone)
    time_t current_time_sec = 0;
    bool ntp_synced = false;
    
    if (handle->ntp_tool) {
        // Use system time() function which returns UTC seconds since epoch
        time(&current_time_sec);
        
        // Check if time is reasonable (after 2020-01-01)
        ntp_synced = (current_time_sec > 1577836800);
    } else {
        // Fallback to esp_timer converted to Unix timestamp
        current_time_sec = esp_timer_get_time() / 1000000ULL;
        ntp_synced = false;
    }

    // Format timestamp as ISO 8601 local time (uses timezone set by NTP tool)
    struct tm *timeinfo = localtime(&current_time_sec);
    if (timeinfo) {
        // Constitutional compliance: Use strftime for safe timestamp formatting
        strftime(payload->rfid_poll_result.timestamp, sizeof(payload->rfid_poll_result.timestamp),
                "%Y-%m-%dT%H:%M:%S", timeinfo);
    } else {
        // Constitutional fallback with safe string copy
        strncpy(payload->rfid_poll_result.timestamp, "1970-01-01T00:00:00", 
                sizeof(payload->rfid_poll_result.timestamp) - 1);
        payload->rfid_poll_result.timestamp[sizeof(payload->rfid_poll_result.timestamp) - 1] = '\0';
    }

    // Set strategic supplemental fields based on configuration
    payload->rfid_poll_result.ntp_synced = ntp_synced;
    
    if (!handle->config.minimal_mode) {
        // Generate session ID (simple approach using timestamp + tag)
        snprintf(payload->rfid_poll_result.session_id, sizeof(payload->rfid_poll_result.session_id),
                 "sess_%lu_%s", (unsigned long)current_time_sec, tag_id);
        
        payload->rfid_poll_result.sequence_number = (uint32_t)(current_time_sec & 0xFFFFFFFF);
        
        // Include network information if enabled
        if (handle->config.include_network_info) {
            payload->rfid_poll_result.wifi_rssi = -50; // TODO: Get actual WiFi RSSI
            snprintf(payload->rfid_poll_result.wifi_status, sizeof(payload->rfid_poll_result.wifi_status), "good");
        }
        
        // Include diagnostic information if enabled
        if (handle->config.include_diagnostics) {
            payload->rfid_poll_result.free_memory_bytes = esp_get_free_heap_size();
            snprintf(payload->rfid_poll_result.firmware_version, sizeof(payload->rfid_poll_result.firmware_version), 
                     PAYLOAD_TOOL_VERSION);
            payload->rfid_poll_result.uptime_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
            payload->rfid_poll_result.processing_time_ms = 10; // TODO: Measure actual processing time
        }
        
        // Set event confidence based on threshold
        float confidence = 0.95f;
        if (confidence * 100 >= handle->config.event_confidence_threshold) {
            payload->rfid_poll_result.event_confidence = confidence;
        } else {
            payload->rfid_poll_result.event_confidence = (float)handle->config.event_confidence_threshold / 100.0f;
        }
    } else {
        // Minimal mode: only set basic session info
        snprintf(payload->rfid_poll_result.session_id, sizeof(payload->rfid_poll_result.session_id),
                 "minimal_%lu", (unsigned long)current_time_sec);
        payload->rfid_poll_result.event_confidence = 1.0f;
    }

    // Set validation status
    payload->is_valid = true;
    payload->creation_timestamp_us = (uint64_t)current_time_sec * 1000000ULL;
    payload->checksum = payload_tool_calculate_checksum(handle, payload);

    ESP_LOGI(TAG, "RFID payload created successfully");
    return ESP_OK;
}

esp_err_t payload_tool_generate_device_id(char *device_id, size_t buffer_size)
{
    if (!device_id || buffer_size < 33) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret == ESP_OK) {
        snprintf(device_id, buffer_size,
                "%02X%02X%02X%02X%02X%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        snprintf(device_id, buffer_size, "UNKNOWN");
    }
    
    return ESP_OK;
}