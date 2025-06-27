/**
 * @file http_tool.h
 * @brief MCP-Inspired HTTP Tool Interface
 * 
 * Transformed from webhook_manager to follow MCP tool composition patterns.
 * Provides handle-based HTTP webhook transmission with event-driven communication.
 * Breaks coupling violations by subscribing to NETWORK_TOOL_EVENTS and RFID_TOOL_EVENTS.
 * Renamed from webhook_tool per process map authority.
 */

#ifndef HTTP_TOOL_H
#define HTTP_TOOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// MCP Tool Metadata & Constants
// =============================================================================

#define HTTP_TOOL_ID            "http"
#define HTTP_TOOL_VERSION       "2.0.0"
#define HTTP_TOOL_DESCRIPTION   "MCP-inspired HTTP webhook tool with event-driven transmission"

#define HTTP_TOOL_MAX_URL_LEN        256
#define HTTP_TOOL_MAX_DEVICE_ID_LEN  33
#define HTTP_TOOL_MAX_TAG_UID_LEN    21
#define HTTP_TOOL_MAX_TAG_TYPE_LEN   17
#define HTTP_TOOL_MAX_LOG_ENTRIES    50
#define HTTP_TOOL_DEFAULT_TIMEOUT_MS 5000

// =============================================================================
// MCP Tool Events System
// =============================================================================

ESP_EVENT_DECLARE_BASE(HTTP_TOOL_EVENTS);

/**
 * @brief Webhook Tool Event Types (Published for other tools)
 */
typedef enum {
    HTTP_TOOL_EVENT_TRANSMISSION_SUCCESS = 0,  ///< HTTP request succeeded
    HTTP_TOOL_EVENT_TRANSMISSION_FAILED,       ///< HTTP request failed
    HTTP_TOOL_EVENT_QUEUE_FULL,                ///< Event queue is full
    HTTP_TOOL_EVENT_CONNECTIVITY_RESTORED,     ///< WiFi restored, processing pending
    HTTP_TOOL_EVENT_RETRY_EXHAUSTED,           ///< Max retries reached for event
    HTTP_TOOL_EVENT_CONFIG_UPDATED,            ///< Configuration changed
} http_tool_event_type_t;

/**
 * @brief Webhook Event Types (Internal)
 */
typedef enum {
    WEBHOOK_EVENT_TAG_PLACED = 0,                 ///< Tag placed on reader
    WEBHOOK_EVENT_TAG_REMOVED,                    ///< Tag removed from reader
} webhook_event_type_t;

/**
 * @brief Webhook Event Data Structure (Internal)
 */
typedef struct {
    webhook_event_type_t event_type;
    char tag_uid[HTTP_TOOL_MAX_TAG_UID_LEN];
    char device_id[HTTP_TOOL_MAX_DEVICE_ID_LEN];
    char tag_type[HTTP_TOOL_MAX_TAG_TYPE_LEN];
    uint32_t timestamp;
    bool sent;
    uint8_t attempts;
    uint32_t next_retry_time_ms;  ///< Next retry time (ms) for exponential backoff
} webhook_event_t;

/**
 * @brief Webhook Tool Event Data Structure (Published)
 */
typedef struct {
    http_tool_event_type_t type;
    union {
        struct {
            char url[HTTP_TOOL_MAX_URL_LEN];
            int status_code;
            uint32_t response_time_ms;
        } transmission_info;
        struct {
            esp_err_t error_code;
            const char* error_message;
            uint8_t retry_count;
        } error_info;
        struct {
            uint32_t pending_count;
            uint32_t success_count;
            uint32_t failed_count;
        } queue_info;
    } data;
} http_tool_event_t;

// =============================================================================
// MCP Tool Capabilities & Configuration
// =============================================================================

/**
 * @brief Webhook Tool Capabilities (Bitmask)
 */
typedef enum {
    HTTP_CAP_HTTP_POST        = (1 << 0),    ///< HTTP POST support
    HTTP_CAP_RETRY_QUEUE      = (1 << 1),    ///< Automatic retry queue
    HTTP_CAP_EVENT_SUBSCRIBE  = (1 << 2),    ///< Event subscription (WiFi/RFID)
    HTTP_CAP_PERSISTENT_LOG   = (1 << 3),    ///< Persistent event logging
    HTTP_CAP_JSON_PAYLOAD     = (1 << 4),    ///< JSON payload generation
    HTTP_CAP_RATE_LIMITING    = (1 << 5),    ///< Rate limiting support
    HTTP_CAP_HEALTH_MONITOR   = (1 << 6),    ///< Connection health monitoring
    HTTP_CAP_AUTO_TRANSMISSION = (1 << 7),   ///< Automatic RFID event transmission
} http_tool_capabilities_t;

/**
 * @brief Webhook Tool Configuration
 */
typedef struct {
    // HTTP Configuration
    char webhook_url[HTTP_TOOL_MAX_URL_LEN];   ///< Target webhook URL
    char device_id[HTTP_TOOL_MAX_DEVICE_ID_LEN]; ///< Device identifier
    uint32_t timeout_ms;                          ///< HTTP request timeout
    
    // Retry Configuration
    uint8_t max_retries;                          ///< Maximum retry attempts
    uint32_t retry_delay_ms;                      ///< Delay between retries
    bool exponential_backoff;                     ///< Enable exponential backoff
    
    // Queue Configuration
    uint16_t max_queue_size;                      ///< Maximum queued events
    uint32_t queue_timeout_ms;                    ///< Queue operation timeout
    
    // File Paths
    char config_file_path[128];                   ///< Configuration file path
    char log_file_path[128];                      ///< Event log file path
    
    // Behavior Settings
    bool auto_save_log;                           ///< Auto-save event log
    bool auto_process_pending;                    ///< Auto-process on WiFi connect
    bool subscribe_to_rfid_events;                ///< Auto-send on RFID events (DEPRECATED - use http_send_payload)
    bool subscribe_to_wifi_events;                ///< Monitor WiFi connectivity
    
    // Event Publishing
    bool publish_events;                          ///< Enable event publishing
    uint32_t event_task_stack_size;               ///< Event task stack size
} http_tool_config_t;

// =============================================================================
// MCP Tool Types & Handles
// =============================================================================

/**
 * @brief Opaque Webhook Tool Handle
 */
typedef struct http_tool_context* http_tool_handle_t;

/**
 * @brief Webhook Tool Status Information
 */
typedef struct {
    bool is_initialized;                          ///< Tool initialization status
    bool is_active;                               ///< Tool active status
    bool wifi_connected;                          ///< WiFi connectivity status
    bool webhook_reachable;                       ///< Webhook server reachable
    char current_url[HTTP_TOOL_MAX_URL_LEN];   ///< Current webhook URL
    uint32_t pending_count;                       ///< Pending events count
    uint32_t success_count;                       ///< Successful transmissions
    uint32_t failed_count;                        ///< Failed transmissions
    uint32_t uptime_ms;                           ///< Tool uptime
    uint32_t last_transmission_ms;                ///< Last successful transmission
    http_tool_capabilities_t capabilities;     ///< Tool capabilities
} http_tool_status_t;

/**
 * @brief Webhook Tool Registry Entry (MCP Pattern)
 */
typedef struct {
    const char* tool_id;
    const char* version;
    const char* description;
    http_tool_capabilities_t capabilities;
    http_tool_handle_t (*init_func)(const http_tool_config_t* config);
    esp_err_t (*deinit_func)(http_tool_handle_t handle);
} http_tool_registry_t;

// =============================================================================
// MCP Tool Interface Functions
// =============================================================================

/**
 * @brief Get webhook tool identifier
 * @return Tool ID string
 */
const char* http_tool_get_id(void);

/**
 * @brief Get webhook tool version
 * @return Version string
 */
const char* http_tool_get_version(void);

/**
 * @brief Create default webhook tool configuration
 * @return Default configuration structure
 */
http_tool_config_t http_tool_create_default_config(void);

/**
 * @brief Initialize webhook tool with configuration
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
http_tool_handle_t http_tool_init(const http_tool_config_t *config);

/**
 * @brief Deinitialize webhook tool and free resources
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_deinit(http_tool_handle_t handle);

/**
 * @brief Get webhook tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
http_tool_capabilities_t http_tool_get_capabilities(http_tool_handle_t handle);

/**
 * @brief Get webhook tool status
 * @param handle Tool handle
 * @param status Pointer to status structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_get_status(http_tool_handle_t handle, http_tool_status_t *status);

/**
 * @brief Get tool registry entry (MCP pattern)
 * @return Pointer to registry entry
 */
const http_tool_registry_t* http_tool_get_registry_entry(void);

// =============================================================================
// Webhook Operations Interface
// =============================================================================

/**
 * @brief Send formatted payload (Process Map Authority - Main->HTTP interface)
 * @param handle Tool handle
 * @param json_payload Formatted JSON payload string
 * @param payload_length Length of JSON payload
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_send_payload(http_tool_handle_t handle,
                           const char* json_payload,
                           size_t payload_length);

/**
 * @brief Send webhook event (manual)
 * @param handle Tool handle
 * @param event_type Event type
 * @param tag_uid Tag UID string
 * @param tag_type Tag type string (optional)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_send_event(http_tool_handle_t handle,
                                   webhook_event_type_t event_type,
                                   const char *tag_uid,
                                   const char *tag_type);

/**
 * @brief Process pending events (manual trigger)
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_process_pending(http_tool_handle_t handle);

/**
 * @brief Get pending events count
 * @param handle Tool handle
 * @param pending_count Pointer to store count
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_get_pending_count(http_tool_handle_t handle, uint32_t *pending_count);

/**
 * @brief Check webhook server connectivity
 * @param handle Tool handle
 * @return ESP_OK if reachable, error code otherwise
 */
esp_err_t http_tool_check_connectivity(http_tool_handle_t handle);

// =============================================================================
// Configuration Management Interface
// =============================================================================

/**
 * @brief Load configuration from file
 * @param handle Tool handle
 * @param file_path Path to configuration file
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_load_config(http_tool_handle_t handle, const char* file_path);

/**
 * @brief Save configuration to file
 * @param handle Tool handle
 * @param file_path Path to configuration file (NULL for default)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_save_config(http_tool_handle_t handle, const char* file_path);

/**
 * @brief Update tool configuration at runtime
 * @param handle Tool handle
 * @param config New configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_update_config(http_tool_handle_t handle, const http_tool_config_t *config);

/**
 * @brief Set device ID for all webhook events
 * @param handle Tool handle
 * @param device_id Device identifier string
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_set_device_id(http_tool_handle_t handle, const char *device_id);

// =============================================================================
// Event Log Management Interface
// =============================================================================

/**
 * @brief Load event log from file
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_load_log(http_tool_handle_t handle);

/**
 * @brief Save event log to file
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_save_log(http_tool_handle_t handle);

/**
 * @brief Clear all logged events
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_tool_clear_log(http_tool_handle_t handle);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert webhook tool event type to string
 * @param event_type Event type
 * @return String representation
 */
const char* http_tool_event_to_string(http_tool_event_type_t event_type);

/**
 * @brief Convert webhook event type to string
 * @param event_type Event type
 * @return String representation
 */
const char* webhook_event_to_string(webhook_event_type_t event_type);

/**
 * @brief Get HTTP status code description
 * @param status_code HTTP status code
 * @return String description
 */
const char* http_tool_http_status_to_string(int status_code);

#ifdef __cplusplus
}
#endif

#endif // HTTP_TOOL_H