/**
 * @file webhook_tool.h
 * @brief MCP-Inspired Webhook Tool Interface
 * 
 * Transformed from webhook_manager to follow MCP tool composition patterns.
 * Provides handle-based HTTP webhook transmission with event-driven communication.
 * Breaks coupling violations by subscribing to WIFI_TOOL_EVENTS and RFID_TOOL_EVENTS.
 */

#ifndef WEBHOOK_TOOL_H
#define WEBHOOK_TOOL_H

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

#define WEBHOOK_TOOL_ID            "webhook"
#define WEBHOOK_TOOL_VERSION       "2.0.0"
#define WEBHOOK_TOOL_DESCRIPTION   "MCP-inspired HTTP webhook tool with event-driven transmission"

#define WEBHOOK_TOOL_MAX_URL_LEN        256
#define WEBHOOK_TOOL_MAX_DEVICE_ID_LEN  33
#define WEBHOOK_TOOL_MAX_TAG_UID_LEN    21
#define WEBHOOK_TOOL_MAX_TAG_TYPE_LEN   17
#define WEBHOOK_TOOL_MAX_LOG_ENTRIES    50
#define WEBHOOK_TOOL_DEFAULT_TIMEOUT_MS 5000

// =============================================================================
// MCP Tool Events System
// =============================================================================

ESP_EVENT_DECLARE_BASE(WEBHOOK_TOOL_EVENTS);

/**
 * @brief Webhook Tool Event Types (Published for other tools)
 */
typedef enum {
    WEBHOOK_TOOL_EVENT_TRANSMISSION_SUCCESS = 0,  ///< HTTP request succeeded
    WEBHOOK_TOOL_EVENT_TRANSMISSION_FAILED,       ///< HTTP request failed
    WEBHOOK_TOOL_EVENT_QUEUE_FULL,                ///< Event queue is full
    WEBHOOK_TOOL_EVENT_CONNECTIVITY_RESTORED,     ///< WiFi restored, processing pending
    WEBHOOK_TOOL_EVENT_RETRY_EXHAUSTED,           ///< Max retries reached for event
    WEBHOOK_TOOL_EVENT_CONFIG_UPDATED,            ///< Configuration changed
} webhook_tool_event_type_t;

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
    char tag_uid[WEBHOOK_TOOL_MAX_TAG_UID_LEN];
    char device_id[WEBHOOK_TOOL_MAX_DEVICE_ID_LEN];
    char tag_type[WEBHOOK_TOOL_MAX_TAG_TYPE_LEN];
    uint32_t timestamp;
    bool sent;
    uint8_t attempts;
} webhook_event_t;

/**
 * @brief Webhook Tool Event Data Structure (Published)
 */
typedef struct {
    webhook_tool_event_type_t type;
    union {
        struct {
            char url[WEBHOOK_TOOL_MAX_URL_LEN];
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
} webhook_tool_event_t;

// =============================================================================
// MCP Tool Capabilities & Configuration
// =============================================================================

/**
 * @brief Webhook Tool Capabilities (Bitmask)
 */
typedef enum {
    WEBHOOK_CAP_HTTP_POST        = (1 << 0),    ///< HTTP POST support
    WEBHOOK_CAP_RETRY_QUEUE      = (1 << 1),    ///< Automatic retry queue
    WEBHOOK_CAP_EVENT_SUBSCRIBE  = (1 << 2),    ///< Event subscription (WiFi/RFID)
    WEBHOOK_CAP_PERSISTENT_LOG   = (1 << 3),    ///< Persistent event logging
    WEBHOOK_CAP_JSON_PAYLOAD     = (1 << 4),    ///< JSON payload generation
    WEBHOOK_CAP_RATE_LIMITING    = (1 << 5),    ///< Rate limiting support
    WEBHOOK_CAP_HEALTH_MONITOR   = (1 << 6),    ///< Connection health monitoring
    WEBHOOK_CAP_AUTO_TRANSMISSION = (1 << 7),   ///< Automatic RFID event transmission
} webhook_tool_capabilities_t;

/**
 * @brief Webhook Tool Configuration
 */
typedef struct {
    // HTTP Configuration
    char webhook_url[WEBHOOK_TOOL_MAX_URL_LEN];   ///< Target webhook URL
    char device_id[WEBHOOK_TOOL_MAX_DEVICE_ID_LEN]; ///< Device identifier
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
    bool subscribe_to_rfid_events;                ///< Auto-send on RFID events
    bool subscribe_to_wifi_events;                ///< Monitor WiFi connectivity
    
    // Event Publishing
    bool publish_events;                          ///< Enable event publishing
    uint32_t event_task_stack_size;               ///< Event task stack size
} webhook_tool_config_t;

// =============================================================================
// MCP Tool Types & Handles
// =============================================================================

/**
 * @brief Opaque Webhook Tool Handle
 */
typedef struct webhook_tool_context* webhook_tool_handle_t;

/**
 * @brief Webhook Tool Status Information
 */
typedef struct {
    bool is_initialized;                          ///< Tool initialization status
    bool is_active;                               ///< Tool active status
    bool wifi_connected;                          ///< WiFi connectivity status
    bool webhook_reachable;                       ///< Webhook server reachable
    char current_url[WEBHOOK_TOOL_MAX_URL_LEN];   ///< Current webhook URL
    uint32_t pending_count;                       ///< Pending events count
    uint32_t success_count;                       ///< Successful transmissions
    uint32_t failed_count;                        ///< Failed transmissions
    uint32_t uptime_ms;                           ///< Tool uptime
    uint32_t last_transmission_ms;                ///< Last successful transmission
    webhook_tool_capabilities_t capabilities;     ///< Tool capabilities
} webhook_tool_status_t;

/**
 * @brief Webhook Tool Registry Entry (MCP Pattern)
 */
typedef struct {
    const char* tool_id;
    const char* version;
    const char* description;
    webhook_tool_capabilities_t capabilities;
    webhook_tool_handle_t (*init_func)(const webhook_tool_config_t* config);
    esp_err_t (*deinit_func)(webhook_tool_handle_t handle);
} webhook_tool_registry_t;

// =============================================================================
// MCP Tool Interface Functions
// =============================================================================

/**
 * @brief Get webhook tool identifier
 * @return Tool ID string
 */
const char* webhook_tool_get_id(void);

/**
 * @brief Get webhook tool version
 * @return Version string
 */
const char* webhook_tool_get_version(void);

/**
 * @brief Create default webhook tool configuration
 * @return Default configuration structure
 */
webhook_tool_config_t webhook_tool_create_default_config(void);

/**
 * @brief Initialize webhook tool with configuration
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
webhook_tool_handle_t webhook_tool_init(const webhook_tool_config_t *config);

/**
 * @brief Deinitialize webhook tool and free resources
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_deinit(webhook_tool_handle_t handle);

/**
 * @brief Get webhook tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
webhook_tool_capabilities_t webhook_tool_get_capabilities(webhook_tool_handle_t handle);

/**
 * @brief Get webhook tool status
 * @param handle Tool handle
 * @param status Pointer to status structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_get_status(webhook_tool_handle_t handle, webhook_tool_status_t *status);

/**
 * @brief Get tool registry entry (MCP pattern)
 * @return Pointer to registry entry
 */
const webhook_tool_registry_t* webhook_tool_get_registry_entry(void);

// =============================================================================
// Webhook Operations Interface
// =============================================================================

/**
 * @brief Send webhook event (manual)
 * @param handle Tool handle
 * @param event_type Event type
 * @param tag_uid Tag UID string
 * @param tag_type Tag type string (optional)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_send_event(webhook_tool_handle_t handle,
                                   webhook_event_type_t event_type,
                                   const char *tag_uid,
                                   const char *tag_type);

/**
 * @brief Process pending events (manual trigger)
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_process_pending(webhook_tool_handle_t handle);

/**
 * @brief Get pending events count
 * @param handle Tool handle
 * @param pending_count Pointer to store count
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_get_pending_count(webhook_tool_handle_t handle, uint32_t *pending_count);

/**
 * @brief Check webhook server connectivity
 * @param handle Tool handle
 * @return ESP_OK if reachable, error code otherwise
 */
esp_err_t webhook_tool_check_connectivity(webhook_tool_handle_t handle);

// =============================================================================
// Configuration Management Interface
// =============================================================================

/**
 * @brief Load configuration from file
 * @param handle Tool handle
 * @param file_path Path to configuration file
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_load_config(webhook_tool_handle_t handle, const char* file_path);

/**
 * @brief Save configuration to file
 * @param handle Tool handle
 * @param file_path Path to configuration file (NULL for default)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_save_config(webhook_tool_handle_t handle, const char* file_path);

/**
 * @brief Update tool configuration at runtime
 * @param handle Tool handle
 * @param config New configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_update_config(webhook_tool_handle_t handle, const webhook_tool_config_t *config);

/**
 * @brief Set device ID for all webhook events
 * @param handle Tool handle
 * @param device_id Device identifier string
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_set_device_id(webhook_tool_handle_t handle, const char *device_id);

// =============================================================================
// Event Log Management Interface
// =============================================================================

/**
 * @brief Load event log from file
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_load_log(webhook_tool_handle_t handle);

/**
 * @brief Save event log to file
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_save_log(webhook_tool_handle_t handle);

/**
 * @brief Clear all logged events
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t webhook_tool_clear_log(webhook_tool_handle_t handle);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert webhook tool event type to string
 * @param event_type Event type
 * @return String representation
 */
const char* webhook_tool_event_to_string(webhook_tool_event_type_t event_type);

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
const char* webhook_tool_http_status_to_string(int status_code);

#ifdef __cplusplus
}
#endif

#endif // WEBHOOK_TOOL_H