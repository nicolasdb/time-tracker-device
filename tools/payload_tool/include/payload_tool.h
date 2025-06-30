/**
 * @file payload_tool.h
 * @brief MCP-Inspired Payload Tool - Event Data Formatting
 * 
 * Centralized tool for creating properly formatted event payloads.
 * Extracts payload formatting logic per process map authority requirements.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// MCP-Inspired Tool Metadata
// =============================================================================

/**
 * @brief Tool identification and capabilities
 */
#define PAYLOAD_TOOL_ID "payload"
#define PAYLOAD_TOOL_VERSION "1.0.0"
#define PAYLOAD_TOOL_DESCRIPTION "Event payload formatting and timestamp coordination"

/**
 * @brief Tool capabilities bitmask
 */
typedef enum {
    PAYLOAD_CAP_JSON_FORMATTING  = (1 << 0),  // JSON payload creation
    PAYLOAD_CAP_TIMESTAMP_CALC   = (1 << 1),  // Precise timestamp calculation
    PAYLOAD_CAP_NTP_INTEGRATION  = (1 << 2),  // NTP offset coordination
    PAYLOAD_CAP_DEVICE_METADATA  = (1 << 3),  // Device information embedding
    PAYLOAD_CAP_BOOT_COUNTER     = (1 << 4),  // Boot counter persistence
    PAYLOAD_CAP_THREAD_SAFE      = (1 << 5)   // Thread-safe operations
} payload_tool_capabilities_t;

// =============================================================================
// Universal Tool Interface (MCP Pattern)
// =============================================================================

/**
 * @brief Opaque tool handle
 */
typedef struct payload_tool* payload_tool_handle_t;

/**
 * @brief Tool configuration structure
 */
typedef struct {
    char device_id[32];              // Device identifier (MAC-based)
    bool enable_boot_counter;        // Enable boot counter persistence
    bool enable_ntp_integration;     // Enable NTP offset calculation
    uint32_t timestamp_precision_us; // Timestamp precision (microseconds)
} payload_tool_config_t;

// =============================================================================
// Event Type Definitions (Process Map Authority)
// =============================================================================

/**
 * @brief Event types as defined in process maps
 */
typedef enum {
    PAYLOAD_EVENT_TAG_PLACED    = 0x01,
    PAYLOAD_EVENT_TAG_REMOVED   = 0x02,
    PAYLOAD_EVENT_SYSTEM_BOOT   = 0x03,
    PAYLOAD_EVENT_SYSTEM_ERROR  = 0x04
} payload_event_type_t;

/**
 * @brief ESP_EVENT payload event IDs (Constitutional Authority)
 */
typedef enum {
    PAYLOAD_EVENT_READY = 0,
    PAYLOAD_EVENT_STORED = 1,
    PAYLOAD_EVENT_TRANSMITTED = 2,
    PAYLOAD_EVENT_FAILED = 3
} payload_event_id_t;

/**
 * @brief Event data structure for payload creation
 */
typedef struct {
    payload_event_type_t event_type;     // Event type
    char tag_uid[32];                    // RFID tag UID (if applicable)
    uint64_t internal_timestamp_us;      // Internal timestamp (microseconds)
    int64_t ntp_offset_ms;               // NTP offset (milliseconds)
    bool ntp_synced;                     // NTP synchronization status
    uint32_t boot_counter;               // Boot counter value
    char additional_data[256];           // Additional event-specific data
} payload_event_data_t;

/**
 * @brief ESP_EVENT payload data structure (Constitutional Authority)
 * Used for esp_event_post() communication between tools
 */
typedef struct {
    char event_type[32];           // "tag_placed", "tag_removed", etc.
    uint64_t internal_timestamp_us;
    bool ntp_synced;
    uint32_t boot_counter;
    char tag_uid[32];
    char additional_data[256];
    char iso_timestamp[32];
} payload_esp_event_data_t;

/**
 * @brief Formatted payload result
 */
typedef struct {
    char* json_string;                   // Formatted JSON payload
    size_t json_length;                  // JSON string length
    char iso_timestamp[32];              // ISO 8601 timestamp string
    bool formatting_success;             // Formatting operation status
} payload_formatted_result_t;

// =============================================================================
// Tool Status and Capabilities
// =============================================================================

/**
 * @brief Tool status structure
 */
typedef struct {
    bool is_initialized;                 // Tool initialization status
    uint32_t payloads_formatted;         // Total payloads formatted
    uint32_t boot_counter_value;         // Current boot counter
    bool ntp_integration_active;         // NTP integration status
    uint32_t last_error_code;            // Last error encountered
} payload_tool_status_t;

// =============================================================================
// MCP Tool Interface Functions
// =============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
payload_tool_config_t payload_tool_create_default_config(void);

/**
 * @brief Initialize payload tool
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
payload_tool_handle_t payload_tool_init(const payload_tool_config_t* config);

/**
 * @brief Cleanup payload tool
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t payload_tool_cleanup(payload_tool_handle_t handle);

/**
 * @brief Get tool identification
 * @return Tool ID string
 */
const char* payload_tool_get_id(void);

/**
 * @brief Get tool version
 * @return Tool version string
 */
const char* payload_tool_get_version(void);

/**
 * @brief Get tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
payload_tool_capabilities_t payload_tool_get_capabilities(payload_tool_handle_t handle);

/**
 * @brief Get tool status
 * @param handle Tool handle
 * @param status Status structure to fill
 * @return ESP_OK on success
 */
esp_err_t payload_tool_get_status(payload_tool_handle_t handle, payload_tool_status_t* status);

// =============================================================================
// Core Payload Formatting Functions (Process Map Authority)
// =============================================================================

/**
 * @brief Format event payload (main function per process maps)
 * @param handle Tool handle
 * @param event_data Event data structure
 * @param result Formatted result structure
 * @return ESP_OK on success
 */
esp_err_t payload_tool_format_event(payload_tool_handle_t handle, 
                                  const payload_event_data_t* event_data,
                                  payload_formatted_result_t* result);

/**
 * @brief Free formatted payload result
 * @param result Result structure to free
 */
void payload_tool_free_result(payload_formatted_result_t* result);

/**
 * @brief Calculate precise timestamp with NTP coordination
 * @param handle Tool handle
 * @param internal_timestamp_us Internal timestamp (microseconds)
 * @param iso_timestamp ISO 8601 string buffer (min 32 bytes)
 * @param ntp_offset_ms NTP offset output (milliseconds)
 * @return ESP_OK on success
 */
esp_err_t payload_tool_calculate_timestamp(payload_tool_handle_t handle,
                                         uint64_t internal_timestamp_us,
                                         char* iso_timestamp,
                                         int64_t* ntp_offset_ms);

/**
 * @brief Get current boot counter value
 * @param handle Tool handle
 * @param boot_counter Boot counter output
 * @return ESP_OK on success
 */
esp_err_t payload_tool_get_boot_counter(payload_tool_handle_t handle, uint32_t* boot_counter);

/**
 * @brief Increment boot counter (called on system boot)
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t payload_tool_increment_boot_counter(payload_tool_handle_t handle);

/**
 * @brief Generate device ID from MAC address
 * @param device_id Buffer for device ID (min 32 bytes)
 * @return ESP_OK on success
 */
esp_err_t payload_tool_generate_device_id(char* device_id);

// =============================================================================
// Event System Integration (Process Map 13 Compliance) 
// =============================================================================

/**
 * @brief Start RFID event subscription per process map 13
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t payload_tool_start_event_subscription(payload_tool_handle_t handle);

// Constitutional compliance: Tool dependencies removed - pure event-driven communication
// Tools communicate via esp_event system only per Process Maps 13 & 14

/**
 * @brief Build and transmit payload per process map 13
 * @param handle Tool handle  
 * @param event_id RFID event ID
 * @param event_data RFID event data
 * @return ESP_OK on success
 */
esp_err_t payload_tool_build_and_transmit_payload(payload_tool_handle_t handle, 
                                                 int event_id, 
                                                 void* event_data);

#ifdef __cplusplus
}
#endif