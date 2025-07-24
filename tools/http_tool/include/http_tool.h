/**
 * @file http_tool.h
 * @brief Constitutional HTTP Tool Interface - Webhook POST & Payload Processing
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Process Map 14 (http_fsm.mmd)
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 * Constitutional Payload Processing: Deferred payload creation with stack safety
 */

#ifndef HTTP_TOOL_H
#define HTTP_TOOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constitutional Tool Metadata
// =============================================================================

#define HTTP_TOOL_ID            "http_tool"
#define HTTP_TOOL_VERSION       "6.1.0"
#define HTTP_TOOL_DESCRIPTION   "Constitutional HTTP webhook POST with payload processing"

#define HTTP_TOOL_MAX_URL_LEN        256
#define HTTP_TOOL_MAX_DEVICE_ID_LEN  33
#define HTTP_TOOL_MAX_PAYLOAD_SIZE   2048
#define HTTP_TOOL_DEFAULT_TIMEOUT_MS 10000

// =============================================================================
// Constitutional ESP_EVENT System
// =============================================================================

ESP_EVENT_DECLARE_BASE(HTTP_TOOL_EVENTS);

/**
 * @brief Constitutional HTTP Tool Event Types
 */
typedef enum {
    HTTP_TOOL_EVENT_PAYLOAD_SENT = 0,      ///< JSON payload successfully sent
    HTTP_TOOL_EVENT_PAYLOAD_FAILED,        ///< Payload transmission failed
    HTTP_TOOL_EVENT_CONNECTED,             ///< HTTP connection established
    HTTP_TOOL_EVENT_DISCONNECTED,          ///< HTTP connection lost
    HTTP_TOOL_EVENT_RETRY_STARTED,         ///< Retry attempt started
    HTTP_TOOL_EVENT_RETRY_EXHAUSTED,       ///< All retries exhausted
    HTTP_TOOL_EVENT_ERROR,                 ///< HTTP error occurred
    HTTP_TOOL_EVENT_READY,                 ///< Tool ready for transmission
} http_tool_event_type_t;

/**
 * @brief Constitutional HTTP Tool Event Data Structure
 */
typedef struct {
    http_tool_event_type_t type;
    uint64_t timestamp_us;
    union {
        struct {
            char url[HTTP_TOOL_MAX_URL_LEN];
            int status_code;
            uint32_t response_time_ms;
            size_t payload_size;
        } transmission_info;
        struct {
            esp_err_t error_code;
            char error_message[128];
            uint8_t retry_count;
        } error_info;
    } data;
    esp_err_t error_code;
    char error_message[64];
} http_tool_event_t;

// =============================================================================
// Constitutional Tool Configuration
// =============================================================================

/**
 * @brief Constitutional HTTP Tool Capabilities (Bitmask)
 */
typedef enum {
    HTTP_CAP_POST_REQUEST     = (1 << 0),  ///< HTTP POST support
    HTTP_CAP_JSON_PAYLOAD     = (1 << 1),  ///< JSON payload handling
    HTTP_CAP_RETRY_LOGIC      = (1 << 2),  ///< Automatic retry on failure
    HTTP_CAP_EVENT_PUBLISH    = (1 << 3),  ///< Event publishing to other tools
    HTTP_CAP_HEALTH_MONITOR   = (1 << 4),  ///< Connection health monitoring
    HTTP_CAP_PAYLOAD_PROCESS  = (1 << 5),  ///< Deferred payload processing
    HTTP_CAP_STACK_SAFETY     = (1 << 6),  ///< Stack-safe payload handling
} http_tool_capabilities_t;

/**
 * @brief Constitutional HTTP Tool Configuration
 */
typedef struct {
    // Server Configuration
    char webhook_url[HTTP_TOOL_MAX_URL_LEN]; ///< Target webhook URL
    char device_id[HTTP_TOOL_MAX_DEVICE_ID_LEN]; ///< Device identifier
    uint32_t timeout_ms;                      ///< HTTP request timeout
    
    // Retry Configuration
    uint8_t max_retries;                      ///< Maximum retry attempts
    uint32_t retry_delay_ms;                  ///< Delay between retries
    bool exponential_backoff;                 ///< Enable exponential backoff
    
    // Payload Processing
    bool enable_payload_processing;           ///< Enable deferred payload processing
    uint32_t max_payload_size;                ///< Maximum payload size
    bool validate_json;                       ///< Validate JSON before sending
    
    // Event Publishing
    bool publish_events;                      ///< Enable event publishing
    uint32_t event_queue_size;                ///< Event queue size
    
    // Constitutional Dependencies
    bool require_network_tool;                ///< Require network connectivity
    bool require_ntp_time;                    ///< Require synchronized time
} http_tool_config_t;

// =============================================================================
// Constitutional Tool Types & Handles
// =============================================================================

/**
 * @brief Opaque Constitutional HTTP Tool Handle
 */
typedef struct http_tool* http_tool_handle_t;

/**
 * @brief Constitutional HTTP Tool Status Information
 */
typedef struct {
    bool is_initialized;                      ///< Tool initialization status
    bool is_active;                           ///< Tool active status
    bool is_connected;                        ///< HTTP connection status
    bool server_reachable;                    ///< Webhook server status
    char current_url[HTTP_TOOL_MAX_URL_LEN];  ///< Current webhook URL
    uint32_t payloads_sent;                   ///< Total payloads sent
    uint32_t payloads_failed;                 ///< Total failed transmissions
    uint32_t retry_count;                     ///< Current retry attempts
    uint64_t uptime_us;                       ///< Tool uptime in microseconds
    http_tool_capabilities_t capabilities;    ///< Tool capabilities
} http_tool_status_t;

// =============================================================================
// Constitutional Tool Interface Functions
// =============================================================================

/**
 * @brief Get HTTP tool identifier
 * @return Tool ID string
 */
const char* http_tool_get_id(void);

/**
 * @brief Get HTTP tool version
 * @return Version string
 */
const char* http_tool_get_version(void);

/**
 * @brief Create default HTTP tool configuration
 * @return Default configuration structure
 */
http_tool_config_t http_tool_create_default_config(void);

/**
 * @brief Initialize constitutional HTTP tool with configuration
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
http_tool_handle_t http_tool_init(const http_tool_config_t *config);

/**
 * @brief Deinitialize constitutional HTTP tool and free resources
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_deinit(http_tool_handle_t handle);

/**
 * @brief Get constitutional HTTP tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
http_tool_capabilities_t http_tool_get_capabilities(http_tool_handle_t handle);

/**
 * @brief Get constitutional HTTP tool status
 * @param handle Tool handle
 * @param status Pointer to status structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_get_status(http_tool_handle_t handle, http_tool_status_t *status);

/**
 * @brief Set tool dependencies for constitutional architecture
 * @param handle HTTP tool handle
 * @param network_tool Network tool handle (for connectivity checks)
 * @param ntp_tool NTP tool handle (for timestamp synchronization)
 * @param payload_tool Payload tool handle (for JSON creation)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_set_dependencies(http_tool_handle_t handle, void* network_tool, void* ntp_tool, void* payload_tool);

// =============================================================================
// Constitutional HTTP Operations Interface
// =============================================================================

/**
 * @brief Process deferred payload with constitutional stack safety
 * @param handle Tool handle
 * @param tag_uid Tag UID string from RFID event
 * @param event_type Event type (APPEARED/DISAPPEARED)
 * @param timestamp_us Constitutional timestamp in microseconds
 * @return ESP_OK on success, error code on failure
 * 
 * Constitutional Authority: This function handles deferred payload processing
 * that was removed from ESP event handlers to prevent stack overflow crashes.
 * Uses large stack context to safely create and transmit JSON payloads.
 */
esp_err_t http_tool_process_deferred_payload(http_tool_handle_t handle,
                                           const char* tag_uid,
                                           const char* event_type,
                                           uint64_t timestamp_us,
                                           const char* session_id);

/**
 * @brief Send formatted JSON payload to webhook server
 * @param handle Tool handle
 * @param json_payload Formatted JSON payload string
 * @param payload_size Size of JSON payload
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_send_payload(http_tool_handle_t handle,
                                const char* json_payload,
                                size_t payload_size);

/**
 * @brief Check webhook server connectivity
 * @param handle Tool handle
 * @return ESP_OK if reachable, error code otherwise
 */
esp_err_t http_tool_check_connectivity(http_tool_handle_t handle);

/**
 * @brief Start HTTP tool transmission capability
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_start(http_tool_handle_t handle);

/**
 * @brief Stop HTTP tool transmission capability
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_stop(http_tool_handle_t handle);

/**
 * @brief Constitutional hardware self-test for HTTP connectivity
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_hardware_self_test(http_tool_handle_t handle);

// =============================================================================
// Constitutional Utility Functions
// =============================================================================

/**
 * @brief Convert HTTP tool event type to string
 * @param event_type Event type
 * @return String representation
 */
const char* http_tool_event_to_string(http_tool_event_type_t event_type);

/**
 * @brief Get HTTP status code description
 * @param status_code HTTP status code
 * @return String description
 */
const char* http_tool_status_to_string(int status_code);

#ifdef __cplusplus
}
#endif

#endif // HTTP_TOOL_H