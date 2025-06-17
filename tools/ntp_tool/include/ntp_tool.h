/**
 * @file ntp_tool.h
 * @brief MCP-Inspired NTP Time Synchronization Tool Interface
 * 
 * Provides handle-based NTP time synchronization with WiFi-triggered updates.
 * Follows established MCP patterns for tool composition and event-driven communication.
 */

#ifndef NTP_TOOL_H
#define NTP_TOOL_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// MCP Tool Metadata & Constants
// =============================================================================

#define NTP_TOOL_ID            "ntp"
#define NTP_TOOL_VERSION       "1.0.0"
#define NTP_TOOL_DESCRIPTION   "MCP-inspired NTP time synchronization tool with WiFi-triggered sync"

#define NTP_TOOL_MAX_SERVERS   4
#define NTP_TOOL_MAX_HOSTNAME  64
#define NTP_TOOL_DEFAULT_SYNC_INTERVAL_S  3600  // 1 hour

// =============================================================================
// MCP Tool Events System
// =============================================================================

ESP_EVENT_DECLARE_BASE(NTP_TOOL_EVENTS);

/**
 * @brief NTP Tool Event Types (Published for other tools)
 */
typedef enum {
    NTP_TOOL_EVENT_SYNC_STARTED = 0,    ///< NTP synchronization started
    NTP_TOOL_EVENT_SYNC_SUCCESS,        ///< Time synchronized successfully
    NTP_TOOL_EVENT_SYNC_FAILED,         ///< Synchronization failed
    NTP_TOOL_EVENT_TIMEZONE_CHANGED,    ///< Timezone configuration changed
    NTP_TOOL_EVENT_TIME_UPDATED,        ///< System time updated
} ntp_tool_event_type_t;

/**
 * @brief NTP Tool Event Data Structure
 */
typedef struct {
    ntp_tool_event_type_t type;
    union {
        struct {
            char server_name[NTP_TOOL_MAX_HOSTNAME];
            uint32_t response_time_ms;
        } sync_info;
        struct {
            time_t old_time;
            time_t new_time;
            int64_t offset_us;  // Time offset in microseconds
        } time_info;
        struct {
            const char* timezone;
            const char* description;
        } timezone_info;
        struct {
            esp_err_t error_code;
            const char* error_message;
        } error_info;
    } data;
} ntp_tool_event_t;

// =============================================================================
// MCP Tool Capabilities & Configuration
// =============================================================================

/**
 * @brief NTP Tool Capabilities (Bitmask)
 */
typedef enum {
    NTP_CAP_TIME_SYNC        = (1 << 0),    ///< Time synchronization support
    NTP_CAP_MULTIPLE_SERVERS = (1 << 1),    ///< Multiple NTP server support
    NTP_CAP_AUTO_SYNC        = (1 << 2),    ///< Automatic synchronization
    NTP_CAP_WIFI_TRIGGERED   = (1 << 3),    ///< WiFi-triggered sync
    NTP_CAP_TIMEZONE_MGMT    = (1 << 4),    ///< Timezone management
    NTP_CAP_EVENT_PUBLISH    = (1 << 5),    ///< Event publishing to other tools
    NTP_CAP_HEALTH_MONITOR   = (1 << 6),    ///< Sync health monitoring
} ntp_tool_capabilities_t;

/**
 * @brief NTP Server Configuration
 */
typedef struct {
    char hostname[NTP_TOOL_MAX_HOSTNAME];    ///< NTP server hostname
    uint8_t priority;                        ///< Server priority (1-255, higher = preferred)
    uint32_t timeout_ms;                     ///< Request timeout in milliseconds
    bool enabled;                            ///< Server enabled flag
} ntp_server_config_t;

/**
 * @brief NTP Tool Configuration
 */
typedef struct {
    // NTP Server Configuration
    ntp_server_config_t servers[NTP_TOOL_MAX_SERVERS];
    uint8_t server_count;
    
    // Synchronization Settings
    uint32_t sync_interval_s;                ///< Auto-sync interval in seconds
    uint32_t sync_timeout_ms;                ///< Sync operation timeout
    uint8_t max_retry_attempts;              ///< Max sync retry attempts
    uint32_t retry_delay_ms;                 ///< Delay between retries
    bool auto_sync_enabled;                  ///< Enable automatic sync
    bool wifi_triggered_sync;                ///< Sync on WiFi connection
    
    // Timezone Configuration
    char timezone[64];                       ///< Timezone string (e.g., "EST5EDT,M3.2.0,M11.1.0")
    char timezone_description[128];          ///< Human-readable timezone description
    
    // Event Publishing
    bool publish_events;                     ///< Enable event publishing
    uint32_t event_stack_size;               ///< Event task stack size
    
    // Health Monitoring
    uint32_t max_drift_threshold_s;          ///< Max acceptable time drift in seconds
    bool drift_monitoring_enabled;           ///< Enable drift monitoring
} ntp_tool_config_t;

// =============================================================================
// MCP Tool Types & Handles
// =============================================================================

/**
 * @brief Opaque NTP Tool Handle
 */
typedef struct ntp_tool_context* ntp_tool_handle_t;

/**
 * @brief NTP Synchronization Status
 */
typedef enum {
    NTP_STATUS_NOT_SYNCED = 0,      ///< Never synchronized
    NTP_STATUS_SYNCING,             ///< Synchronization in progress
    NTP_STATUS_SYNCED,              ///< Successfully synchronized
    NTP_STATUS_SYNC_FAILED,         ///< Last synchronization failed
    NTP_STATUS_DRIFT_WARNING,       ///< Time drift detected
} ntp_sync_status_t;

/**
 * @brief NTP Tool Status Information
 */
typedef struct {
    bool is_initialized;                     ///< Tool initialization status
    bool is_active;                          ///< Tool active status
    ntp_sync_status_t sync_status;           ///< Current sync status
    time_t last_sync_time;                   ///< Last successful sync timestamp
    time_t next_sync_time;                   ///< Next scheduled sync timestamp
    int64_t last_offset_us;                  ///< Last time offset in microseconds
    uint32_t sync_attempts;                  ///< Total sync attempts
    uint32_t successful_syncs;               ///< Successful sync count
    uint32_t failed_syncs;                   ///< Failed sync count
    char active_server[NTP_TOOL_MAX_HOSTNAME]; ///< Currently active server
    uint32_t uptime_ms;                      ///< Tool uptime
    ntp_tool_capabilities_t capabilities;    ///< Tool capabilities
} ntp_tool_status_t;

/**
 * @brief NTP Tool Registry Entry (MCP Pattern)
 */
typedef struct {
    const char* tool_id;
    const char* version;
    const char* description;
    ntp_tool_capabilities_t capabilities;
    ntp_tool_handle_t (*init_func)(const ntp_tool_config_t* config);
    esp_err_t (*deinit_func)(ntp_tool_handle_t handle);
} ntp_tool_registry_t;

// =============================================================================
// MCP Tool Interface Functions
// =============================================================================

/**
 * @brief Get NTP tool identifier
 * @return Tool ID string
 */
const char* ntp_tool_get_id(void);

/**
 * @brief Get NTP tool version
 * @return Version string
 */
const char* ntp_tool_get_version(void);

/**
 * @brief Create default NTP tool configuration
 * @return Default configuration structure
 */
ntp_tool_config_t ntp_tool_create_default_config(void);

/**
 * @brief Initialize NTP tool with configuration
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
ntp_tool_handle_t ntp_tool_init(const ntp_tool_config_t *config);

/**
 * @brief Deinitialize NTP tool and free resources
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_deinit(ntp_tool_handle_t handle);

/**
 * @brief Get NTP tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
ntp_tool_capabilities_t ntp_tool_get_capabilities(ntp_tool_handle_t handle);

/**
 * @brief Get NTP tool status
 * @param handle Tool handle
 * @param status Pointer to status structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_get_status(ntp_tool_handle_t handle, ntp_tool_status_t *status);

/**
 * @brief Get tool registry entry (MCP pattern)
 * @return Pointer to registry entry
 */
const ntp_tool_registry_t* ntp_tool_get_registry_entry(void);

// =============================================================================
// Time Synchronization Interface
// =============================================================================

/**
 * @brief Start NTP synchronization manually
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_sync_now(ntp_tool_handle_t handle);

/**
 * @brief Enable/disable automatic synchronization
 * @param handle Tool handle
 * @param enable True to enable, false to disable
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_set_auto_sync(ntp_tool_handle_t handle, bool enable);

/**
 * @brief Update sync interval
 * @param handle Tool handle
 * @param interval_s New sync interval in seconds
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_set_sync_interval(ntp_tool_handle_t handle, uint32_t interval_s);

/**
 * @brief Get current system time with high precision
 * @param handle Tool handle
 * @param tv Pointer to timeval structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_get_time(ntp_tool_handle_t handle, struct timeval *tv);

/**
 * @brief Get current system time as Unix timestamp
 * @param handle Tool handle
 * @param timestamp Pointer to timestamp
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_get_timestamp(ntp_tool_handle_t handle, time_t *timestamp);

// =============================================================================
// Configuration Management Interface
// =============================================================================

/**
 * @brief Add NTP server to configuration
 * @param handle Tool handle
 * @param server Server configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_add_server(ntp_tool_handle_t handle, const ntp_server_config_t *server);

/**
 * @brief Remove NTP server from configuration
 * @param handle Tool handle
 * @param hostname Server hostname to remove
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_remove_server(ntp_tool_handle_t handle, const char* hostname);

/**
 * @brief Set timezone
 * @param handle Tool handle
 * @param timezone Timezone string
 * @param description Human-readable description
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_set_timezone(ntp_tool_handle_t handle, const char* timezone, const char* description);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert NTP tool event type to string
 * @param event_type Event type
 * @return String representation
 */
const char* ntp_tool_event_to_string(ntp_tool_event_type_t event_type);

/**
 * @brief Convert sync status to string
 * @param status Sync status
 * @return String representation
 */
const char* ntp_tool_status_to_string(ntp_sync_status_t status);

/**
 * @brief Format timestamp to human-readable string
 * @param timestamp Unix timestamp
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ntp_tool_format_time(time_t timestamp, char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // NTP_TOOL_H