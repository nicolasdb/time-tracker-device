/**
 * @file payload_tool.h
 * @brief Constitutional Payload Tool Interface - JSON Data Formatting
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

#ifndef PAYLOAD_TOOL_H
#define PAYLOAD_TOOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constitutional Tool Metadata
// =============================================================================

#define PAYLOAD_TOOL_ID            "payload_tool"
#define PAYLOAD_TOOL_VERSION       "6.1.0"
#define PAYLOAD_TOOL_DESCRIPTION   "Constitutional payload formatting with JSON structure"

#define PAYLOAD_TOOL_MAX_PAYLOAD_SIZE    1024
#define PAYLOAD_TOOL_MAX_RFID_UID_LEN    20
#define PAYLOAD_TOOL_MAX_DEVICE_ID_LEN   32

// =============================================================================
// Constitutional ESP_EVENT System
// =============================================================================

ESP_EVENT_DECLARE_BASE(PAYLOAD_TOOL_EVENTS);

/**
 * @brief Constitutional Payload Tool Event Types
 */
typedef enum {
    PAYLOAD_TOOL_EVENT_CREATED = 0,        ///< Payload created successfully
    PAYLOAD_TOOL_EVENT_FORMATTED,          ///< Payload formatted to JSON
    PAYLOAD_TOOL_EVENT_VALIDATED,          ///< Payload validation passed
    PAYLOAD_TOOL_EVENT_ERROR,              ///< Payload processing error
    PAYLOAD_TOOL_EVENT_READY,              ///< Tool ready for payload creation
} payload_tool_event_type_t;

/**
 * @brief Constitutional Session Types
 */
typedef enum {
    PAYLOAD_SESSION_START = 0,             ///< Work session start event
    PAYLOAD_SESSION_END,                   ///< Work session end event
    PAYLOAD_SESSION_ACTIVE,                ///< Ongoing session heartbeat
    PAYLOAD_SESSION_UNKNOWN                ///< Unknown session type
} payload_session_type_t;

/**
 * @brief Constitutional Work Session Data
 */
typedef struct {
    payload_session_type_t type;           ///< Session event type
    char rfid_uid[PAYLOAD_TOOL_MAX_RFID_UID_LEN + 1]; ///< RFID tag UID
    uint64_t timestamp_us;                 ///< Event timestamp (microseconds)
    uint32_t session_duration_ms;          ///< Session duration (for end events)
    char project_id[32];                   ///< Project identifier
    char task_description[128];            ///< Task description
} payload_work_session_t;

/**
 * @brief Constitutional Device Metadata
 */
typedef struct {
    char device_id[PAYLOAD_TOOL_MAX_DEVICE_ID_LEN + 1]; ///< Unique device identifier
    char firmware_version[16];             ///< Firmware version string
    char hardware_revision[16];            ///< Hardware revision
    uint64_t uptime_us;                    ///< Device uptime in microseconds
    uint32_t free_memory_bytes;            ///< Available memory
    int8_t wifi_rssi;                      ///< WiFi signal strength
} payload_device_metadata_t;

/**
 * @brief Constitutional Payload Structure
 */
typedef struct {
    // Core session data
    payload_work_session_t session;
    
    // Device metadata
    payload_device_metadata_t device;
    
    // Formatted output
    char json_payload[PAYLOAD_TOOL_MAX_PAYLOAD_SIZE];
    size_t json_size;
    
    // Validation status
    bool is_valid;
    uint32_t checksum;
    uint64_t creation_timestamp_us;
} payload_data_t;

/**
 * @brief Constitutional Payload Tool Event Data Structure
 */
typedef struct {
    payload_tool_event_type_t type;
    payload_data_t payload;
    uint64_t timestamp_us;
    esp_err_t error_code;
    char error_message[64];
} payload_tool_event_t;

// =============================================================================
// Constitutional Tool Configuration
// =============================================================================

/**
 * @brief Constitutional Payload Tool Capabilities (Bitmask)
 */
typedef enum {
    PAYLOAD_CAP_JSON_FORMAT      = (1 << 0),  ///< JSON formatting support
    PAYLOAD_CAP_SESSION_TRACKING = (1 << 1),  ///< Work session tracking
    PAYLOAD_CAP_DEVICE_METADATA  = (1 << 2),  ///< Device metadata inclusion
    PAYLOAD_CAP_VALIDATION       = (1 << 3),  ///< Payload validation
    PAYLOAD_CAP_COMPRESSION      = (1 << 4),  ///< Data compression
    PAYLOAD_CAP_ENCRYPTION       = (1 << 5),  ///< Data encryption
    PAYLOAD_CAP_BATCHING         = (1 << 6),  ///< Batch processing
} payload_tool_capabilities_t;

/**
 * @brief Constitutional Payload Tool Configuration
 */
typedef struct {
    // Output formatting
    bool include_device_metadata;          ///< Include device info in payload
    bool include_debug_info;               ///< Include debug information
    bool compress_payload;                 ///< Enable payload compression
    bool encrypt_payload;                  ///< Enable payload encryption
    
    // Session tracking
    bool enable_session_tracking;          ///< Enable work session detection
    uint32_t min_session_duration_ms;      ///< Minimum valid session duration
    uint32_t max_session_duration_ms;      ///< Maximum valid session duration
    
    // Validation settings
    bool enable_payload_validation;        ///< Enable payload validation
    bool require_ntp_sync;                 ///< Require NTP time synchronization
    
    // Event publishing
    bool publish_events;                   ///< Enable event publishing
    uint32_t event_queue_size;             ///< Event queue size
} payload_tool_config_t;

// =============================================================================
// Constitutional Tool Types & Handles
// =============================================================================

/**
 * @brief Opaque Constitutional Payload Tool Handle
 */
typedef struct payload_tool* payload_tool_handle_t;

/**
 * @brief Constitutional Payload Tool Status Information
 */
typedef struct {
    bool is_initialized;                   ///< Tool initialization status
    bool is_active;                        ///< Tool active status
    bool ntp_sync_required;                ///< NTP synchronization required
    bool validation_enabled;               ///< Payload validation enabled
    uint32_t payloads_created;             ///< Total payloads created
    uint32_t validation_errors;            ///< Total validation errors
    uint32_t format_errors;                ///< Total formatting errors
    uint64_t uptime_us;                    ///< Tool uptime in microseconds
    payload_tool_capabilities_t capabilities; ///< Tool capabilities
} payload_tool_status_t;

// =============================================================================
// Constitutional Tool Interface Functions
// =============================================================================

/**
 * @brief Get payload tool identifier
 * @return Tool ID string
 */
const char* payload_tool_get_id(void);

/**
 * @brief Get payload tool version
 * @return Version string
 */
const char* payload_tool_get_version(void);

/**
 * @brief Create default payload tool configuration
 * @return Default configuration structure
 */
payload_tool_config_t payload_tool_create_default_config(void);

/**
 * @brief Initialize constitutional payload tool with configuration
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
payload_tool_handle_t payload_tool_init(const payload_tool_config_t *config);

/**
 * @brief Deinitialize constitutional payload tool and free resources
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_deinit(payload_tool_handle_t handle);

/**
 * @brief Get constitutional payload tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
payload_tool_capabilities_t payload_tool_get_capabilities(payload_tool_handle_t handle);

/**
 * @brief Get constitutional payload tool status
 * @param handle Tool handle
 * @param status Pointer to status structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_get_status(payload_tool_handle_t handle, payload_tool_status_t *status);

/**
 * @brief Set NTP tool dependency for timestamp generation
 * @param handle Payload tool handle
 * @param ntp_tool NTP tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_set_ntp_dependency(payload_tool_handle_t handle, void* ntp_tool);

/**
 * @brief Set filesystem tool dependency for payload storage
 * @param handle Payload tool handle
 * @param fs_tool Filesystem tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_set_fs_dependency(payload_tool_handle_t handle, void* fs_tool);

// =============================================================================
// Constitutional Payload Operations Interface
// =============================================================================

/**
 * @brief Create constitutional work session payload
 * @param handle Tool handle
 * @param session Work session data
 * @param payload Output payload structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_create_session_payload(payload_tool_handle_t handle, 
                                             const payload_work_session_t *session,
                                             payload_data_t *payload);

/**
 * @brief Format payload to JSON string
 * @param handle Tool handle
 * @param payload Payload data to format
 * @param json_buffer Output JSON buffer
 * @param buffer_size Size of JSON buffer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_format_to_json(payload_tool_handle_t handle,
                                     const payload_data_t *payload,
                                     char *json_buffer,
                                     size_t buffer_size);

/**
 * @brief Validate constitutional payload structure
 * @param handle Tool handle
 * @param payload Payload to validate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_validate_payload(payload_tool_handle_t handle,
                                       const payload_data_t *payload);

/**
 * @brief Get current device metadata
 * @param handle Tool handle
 * @param metadata Output device metadata structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_get_device_metadata(payload_tool_handle_t handle,
                                          payload_device_metadata_t *metadata);

/**
 * @brief Calculate payload checksum for validation
 * @param handle Tool handle
 * @param payload Payload data
 * @return Checksum value
 */
uint32_t payload_tool_calculate_checksum(payload_tool_handle_t handle,
                                        const payload_data_t *payload);

/**
 * @brief Constitutional hardware self-test for payload creation
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_hardware_self_test(payload_tool_handle_t handle);

/**
 * @brief Process batch of session events
 * @param handle Tool handle
 * @param sessions Array of session events
 * @param session_count Number of sessions
 * @param batch_payload Output batch payload
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_create_batch_payload(payload_tool_handle_t handle,
                                           const payload_work_session_t *sessions,
                                           size_t session_count,
                                           payload_data_t *batch_payload);

// =============================================================================
// Constitutional Utility Functions
// =============================================================================

/**
 * @brief Convert payload tool event type to string
 * @param event_type Event type
 * @return String representation
 */
const char* payload_tool_event_to_string(payload_tool_event_type_t event_type);

/**
 * @brief Convert session type to string
 * @param session_type Session type
 * @return String representation  
 */
const char* payload_tool_session_type_to_string(payload_session_type_t session_type);

/**
 * @brief Generate device ID from hardware MAC address
 * @param device_id Output buffer for device ID (minimum 33 chars)
 * @param buffer_size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t payload_tool_generate_device_id(char *device_id, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // PAYLOAD_TOOL_H