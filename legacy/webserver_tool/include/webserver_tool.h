/**
 * @file webserver_tool.h
 * @brief MCP-Inspired Webserver Tool for AP Mode Configuration
 * 
 * Provides HTTP server with REST API for WiFi configuration during AP mode.
 * Integrates with fs_tool for persistent storage and wifi_tool for configuration.
 */

#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Tool Identification & Version
// =============================================================================

#define WEBSERVER_TOOL_ID "webserver_tool"
#define WEBSERVER_TOOL_VERSION "1.0.0"
#define WEBSERVER_TOOL_DESCRIPTION "MCP Webserver Tool for AP Mode Configuration"

// =============================================================================
// Tool Capabilities (Bitmask)
// =============================================================================

typedef enum {
    WEBSERVER_CAP_HTTP_SERVER     = (1 << 0),  ///< HTTP server functionality
    WEBSERVER_CAP_REST_API        = (1 << 1),  ///< REST API endpoints
    WEBSERVER_CAP_STATIC_FILES    = (1 << 2),  ///< Static file serving
    WEBSERVER_CAP_WIFI_CONFIG     = (1 << 3),  ///< WiFi configuration API
    WEBSERVER_CAP_CAPTIVE_PORTAL  = (1 << 4),  ///< Captive portal support
    WEBSERVER_CAP_EVENT_PUBLISH   = (1 << 5),  ///< Event publishing
} webserver_tool_capabilities_t;

// =============================================================================
// Tool Configuration
// =============================================================================

typedef struct {
    // HTTP Server Configuration
    uint16_t port;                  ///< Server port (default: 80)
    uint8_t max_connections;        ///< Max concurrent connections
    uint8_t backlog_conn;           ///< Backlog connections
    uint8_t max_uri_handlers;       ///< Max URI handlers
    uint8_t max_resp_headers;       ///< Max response headers
    uint8_t max_open_sockets;       ///< Max open sockets
    uint16_t stack_size;            ///< Server task stack size
    uint8_t task_priority;          ///< Server task priority
    bool lru_purge_enable;          ///< Enable LRU purge
    uint8_t recv_wait_timeout;      ///< Receive timeout (seconds)
    uint8_t send_wait_timeout;      ///< Send timeout (seconds)
    
    // Behavior Settings
    bool auto_start;                ///< Auto-start server on init
    bool publish_events;            ///< Enable event publishing
    bool enable_cors;               ///< Enable CORS headers
} webserver_tool_config_t;

// =============================================================================
// Tool Types & Handles
// =============================================================================

/**
 * @brief Opaque Webserver Tool Handle
 */
typedef struct webserver_tool_context* webserver_tool_handle_t;

/**
 * @brief Forward declaration for fs_tool dependency injection
 */
typedef struct fs_tool_context* fs_tool_handle_t;

// Forward declarations for fs_tool APIs (MCP dependency injection pattern)
// Note: cJSON forward declaration
typedef struct cJSON cJSON;
esp_err_t fs_tool_load_json_config(fs_tool_handle_t handle, const char *filename, cJSON **json_object);
esp_err_t fs_tool_save_json_config(fs_tool_handle_t handle, const char *filename, const cJSON *json_object);

/**
 * @brief Webserver Tool Status Information
 */
typedef struct {
    bool is_initialized;            ///< Tool initialization status
    bool is_active;                 ///< Tool active status
    bool server_running;            ///< HTTP server running status
    uint16_t port;                  ///< Current server port
    uint32_t uptime_ms;             ///< Tool uptime in milliseconds
} webserver_tool_status_t;

// =============================================================================
// Tool Events
// =============================================================================

ESP_EVENT_DECLARE_BASE(WEBSERVER_TOOL_EVENTS);

typedef enum {
    WEBSERVER_TOOL_EVENT_STARTED,           ///< Server started
    WEBSERVER_TOOL_EVENT_STOPPED,           ///< Server stopped
    WEBSERVER_TOOL_EVENT_CLIENT_CONNECTED,  ///< Client connected
    WEBSERVER_TOOL_EVENT_CLIENT_DISCONNECTED, ///< Client disconnected
    WEBSERVER_TOOL_EVENT_CONFIG_UPDATED,    ///< WiFi config updated
    WEBSERVER_TOOL_EVENT_RESTART_REQUESTED, ///< Restart requested via API
} webserver_tool_event_type_t;

typedef struct {
    webserver_tool_event_type_t type;
    union {
        struct {
            char client_ip[16];
        } client_info;
        struct {
            char config_name[32];
        } config_info;
    } data;
} webserver_tool_event_t;

// =============================================================================
// Tool Registry (MCP Pattern)
// =============================================================================

typedef struct {
    const char* tool_id;
    const char* version;
    const char* description;
    webserver_tool_capabilities_t capabilities;
    webserver_tool_handle_t (*init_func)(const webserver_tool_config_t*);
    esp_err_t (*deinit_func)(webserver_tool_handle_t);
} webserver_tool_registry_t;

// =============================================================================
// MCP Tool Interface
// =============================================================================

/**
 * @brief Get tool identifier
 * @return Tool ID string
 */
const char* webserver_tool_get_id(void);

/**
 * @brief Get tool version
 * @return Version string
 */
const char* webserver_tool_get_version(void);

/**
 * @brief Create default tool configuration
 * @return Default configuration structure
 */
webserver_tool_config_t webserver_tool_create_default_config(void);

/**
 * @brief Initialize webserver tool
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
webserver_tool_handle_t webserver_tool_init(const webserver_tool_config_t *config);

/**
 * @brief Deinitialize webserver tool
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t webserver_tool_deinit(webserver_tool_handle_t handle);

/**
 * @brief Set filesystem tool dependency (MCP dependency injection)
 * @param handle Webserver tool handle
 * @param fs_tool Filesystem tool handle
 * @return ESP_OK on success
 */
esp_err_t webserver_tool_set_fs_dependency(webserver_tool_handle_t handle, fs_tool_handle_t fs_tool);

/**
 * @brief Start HTTP server
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t webserver_tool_start(webserver_tool_handle_t handle);

/**
 * @brief Stop HTTP server
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t webserver_tool_stop(webserver_tool_handle_t handle);

/**
 * @brief Get tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
webserver_tool_capabilities_t webserver_tool_get_capabilities(webserver_tool_handle_t handle);

/**
 * @brief Get tool status
 * @param handle Tool handle
 * @param status Pointer to status structure
 * @return ESP_OK on success
 */
esp_err_t webserver_tool_get_status(webserver_tool_handle_t handle, webserver_tool_status_t *status);

/**
 * @brief Get tool registry entry (MCP pattern)
 * @return Pointer to registry entry
 */
const webserver_tool_registry_t* webserver_tool_get_registry_entry(void);

#ifdef __cplusplus
}
#endif