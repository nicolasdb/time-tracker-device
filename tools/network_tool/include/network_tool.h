/**
 * @file network_tool.h
 * @brief MCP-Inspired Network Tool Interface
 * 
 * Transformed from wifi_manager to follow MCP tool composition patterns.
 * Provides handle-based network management with event-driven communication.
 * Renamed from wifi_tool per process map authority.
 */

#ifndef NETWORK_TOOL_H
#define NETWORK_TOOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for MCP tool dependencies
typedef struct fs_tool_context* fs_tool_handle_t;
typedef struct cJSON cJSON;

// =============================================================================
// MCP Tool Metadata & Constants
// =============================================================================

#define NETWORK_TOOL_ID         "network"
#define NETWORK_TOOL_VERSION       "2.0.0"
#define NETWORK_TOOL_DESCRIPTION   "MCP-inspired WiFi connectivity tool with multi-network support"

#define NETWORK_TOOL_MAX_NETWORKS  5
#define NETWORK_TOOL_MAX_SSID_LEN  32
#define NETWORK_TOOL_MAX_PASS_LEN  64

// =============================================================================
// MCP Tool Events System
// =============================================================================

ESP_EVENT_DECLARE_BASE(NETWORK_TOOL_EVENTS);

/**
 * @brief WiFi Tool Event Types (Published for other tools)
 */
typedef enum {
    NETWORK_TOOL_EVENT_STA_CONNECTING = 0,     ///< Attempting to connect to network
    NETWORK_TOOL_EVENT_STA_CONNECTED,          ///< Successfully connected to network
    NETWORK_TOOL_EVENT_STA_DISCONNECTED,       ///< Disconnected from network
    NETWORK_TOOL_EVENT_STA_FAILED,             ///< Connection attempt failed
    NETWORK_TOOL_EVENT_AP_STARTED,             ///< AP mode started and ready
    NETWORK_TOOL_EVENT_AP_STOPPED,             ///< AP mode stopped
    NETWORK_TOOL_EVENT_AP_CLIENT_CONNECTED,    ///< Client connected to our AP
    NETWORK_TOOL_EVENT_AP_CLIENT_DISCONNECTED, ///< Client disconnected from our AP
    NETWORK_TOOL_EVENT_CONFIG_CHANGED,         ///< Network configuration updated
    NETWORK_TOOL_EVENT_IP_ACQUIRED,            ///< Got IP address (STA mode)
    NETWORK_TOOL_EVENT_IP_LOST,                ///< Lost IP address
} network_tool_event_type_t;

/**
 * @brief WiFi Tool Event Data Structure
 */
typedef struct {
    network_tool_event_type_t type;
    union {
        struct {
            char ssid[NETWORK_TOOL_MAX_SSID_LEN];
            uint8_t bssid[6];
            int8_t rssi;
        } sta_info;
        struct {
            char ip_address[16];     // "192.168.1.100"
            char gateway[16];        // "192.168.1.1"
            char netmask[16];        // "255.255.255.0"
        } ip_info;
        struct {
            char ap_ssid[NETWORK_TOOL_MAX_SSID_LEN];
            char ip_address[16];     // "192.168.4.1"
            uint8_t channel;
            uint8_t client_count;
        } ap_info;
        struct {
            wifi_err_reason_t reason;
            uint8_t retry_count;
        } error_info;
    } data;
} network_tool_event_t;

// =============================================================================
// MCP Tool Capabilities & Configuration
// =============================================================================

/**
 * @brief WiFi Tool Capabilities (Bitmask)
 */
typedef enum {
    NETWORK_CAP_STA_MODE        = (1 << 0),    ///< Station mode support
    NETWORK_CAP_AP_MODE         = (1 << 1),    ///< Access Point mode support
    NETWORK_CAP_MULTI_NETWORK   = (1 << 2),    ///< Multiple network configuration
    NETWORK_CAP_AUTO_RECONNECT  = (1 << 3),    ///< Automatic reconnection
    NETWORK_CAP_CONFIG_PERSIST  = (1 << 4),    ///< Configuration persistence
    NETWORK_CAP_EVENT_PUBLISH   = (1 << 5),    ///< Event publishing to other tools
    NETWORK_CAP_HEALTH_MONITOR  = (1 << 6),    ///< Connection health monitoring
    NETWORK_CAP_CONCURRENT_MODE = (1 << 7),    ///< STA+AP concurrent mode
} network_tool_capabilities_t;

/**
 * @brief Network Configuration Entry
 */
typedef struct {
    char ssid[NETWORK_TOOL_MAX_SSID_LEN];
    char password[NETWORK_TOOL_MAX_PASS_LEN];
    uint8_t priority;                        ///< 1-255, higher = preferred
    wifi_auth_mode_t auth_mode;              ///< Security mode
    bool hidden;                             ///< Hidden network flag
} wifi_network_config_t;

/**
 * @brief WiFi Tool AP Mode Configuration
 */
typedef struct {
    char ssid[NETWORK_TOOL_MAX_SSID_LEN];
    char password[NETWORK_TOOL_MAX_PASS_LEN];
    uint8_t channel;                         ///< WiFi channel (1-13)
    uint8_t max_connections;                 ///< Max concurrent clients
    wifi_auth_mode_t auth_mode;              ///< AP security mode
    bool ssid_hidden;                        ///< Hide SSID
    char ip_address[16];                     ///< AP IP address
    char gateway[16];                        ///< Gateway IP
    char netmask[16];                        ///< Network mask
} network_tool_ap_config_t;

/**
 * @brief WiFi Tool Configuration
 */
typedef struct {
    // Network Configuration
    wifi_network_config_t networks[NETWORK_TOOL_MAX_NETWORKS];
    uint8_t network_count;
    
    // AP Mode Configuration
    network_tool_ap_config_t ap_config;
    
    // Behavior Settings
    uint32_t connect_timeout_ms;             ///< STA connection timeout
    uint8_t max_retry_attempts;              ///< Max connection retries
    uint32_t retry_delay_ms;                 ///< Delay between retries
    bool auto_reconnect;                     ///< Auto-reconnect on disconnect
    bool enable_ap_fallback;                 ///< Start AP if all STA fails
    
    // Configuration Persistence
    char config_file_path[128];              ///< JSON config file path
    bool auto_save_config;                   ///< Auto-save on changes
    
    // Event Publishing
    bool publish_events;                     ///< Enable event publishing
    uint32_t event_stack_size;               ///< Event task stack size
} network_tool_config_t;

// =============================================================================
// MCP Tool Types & Handles
// =============================================================================

/**
 * @brief Opaque WiFi Tool Handle
 */
typedef struct network_tool_context* network_tool_handle_t;

/**
 * @brief WiFi Tool Status Information
 */
typedef struct {
    bool is_initialized;                     ///< Tool initialization status
    bool is_active;                          ///< Tool active status
    bool sta_connected;                      ///< STA mode connection status
    bool ap_active;                          ///< AP mode active status
    char current_ssid[NETWORK_TOOL_MAX_SSID_LEN]; ///< Connected SSID
    char ip_address[16];                     ///< Current IP address
    int8_t rssi;                             ///< Signal strength (dBm)
    uint8_t retry_count;                     ///< Current retry attempt
    uint32_t uptime_ms;                      ///< Tool uptime
    uint8_t ap_client_count;                 ///< Connected AP clients
    network_tool_capabilities_t capabilities;   ///< Tool capabilities
} network_tool_status_t;

/**
 * @brief WiFi Tool Registry Entry (MCP Pattern)
 */
typedef struct {
    const char* tool_id;
    const char* version;
    const char* description;
    network_tool_capabilities_t capabilities;
    network_tool_handle_t (*init_func)(const network_tool_config_t* config);
    esp_err_t (*deinit_func)(network_tool_handle_t handle);
} network_tool_registry_t;

// =============================================================================
// MCP Tool Interface Functions
// =============================================================================

/**
 * @brief Get WiFi tool identifier
 * @return Tool ID string
 */
const char* network_tool_get_id(void);

/**
 * @brief Get WiFi tool version
 * @return Version string
 */
const char* network_tool_get_version(void);

/**
 * @brief Create default WiFi tool configuration
 * @return Default configuration structure
 */
network_tool_config_t network_tool_create_default_config(void);

/**
 * @brief Initialize WiFi tool with configuration
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
network_tool_handle_t network_tool_init(const network_tool_config_t *config);

/**
 * @brief Deinitialize WiFi tool and free resources
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_deinit(network_tool_handle_t handle);

/**
 * @brief Get WiFi tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
network_tool_capabilities_t network_tool_get_capabilities(network_tool_handle_t handle);

/**
 * @brief Get WiFi tool status
 * @param handle Tool handle
 * @param status Pointer to status structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_get_status(network_tool_handle_t handle, network_tool_status_t *status);

/**
 * @brief Get tool registry entry (MCP pattern)
 * @return Pointer to registry entry
 */
const network_tool_registry_t* network_tool_get_registry_entry(void);

/**
 * @brief Set filesystem tool dependency (MCP dependency injection)
 * @param handle WiFi tool handle
 * @param fs_handle Filesystem tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_set_fs_dependency(network_tool_handle_t handle, fs_tool_handle_t fs_handle);

/**
 * @brief Start automatic WiFi connection if networks are configured
 * @param handle WiFi tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_start_auto_connection(network_tool_handle_t handle);

/**
 * @brief Load WiFi networks from JSON configuration
 * @param handle WiFi tool handle
 * @param wifi_config JSON configuration object
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_load_networks_from_json(network_tool_handle_t handle, const cJSON *wifi_config);

// =============================================================================
// WiFi Operations Interface
// =============================================================================

/**
 * @brief Start WiFi in Station mode
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_start_sta(network_tool_handle_t handle);

/**
 * @brief Start WiFi in AP mode
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_start_ap(network_tool_handle_t handle);

/**
 * @brief Stop all WiFi operations
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_stop(network_tool_handle_t handle);

/**
 * @brief Connect to specific network (STA mode)
 * @param handle Tool handle
 * @param ssid Network SSID
 * @param password Network password (can be NULL for open networks)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_connect(network_tool_handle_t handle, const char* ssid, const char* password);

/**
 * @brief Disconnect from current network
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_disconnect(network_tool_handle_t handle);

/**
 * @brief Add network to configuration
 * @param handle Tool handle
 * @param network Network configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_add_network(network_tool_handle_t handle, const wifi_network_config_t *network);

/**
 * @brief Remove network from configuration
 * @param handle Tool handle
 * @param ssid SSID to remove
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_remove_network(network_tool_handle_t handle, const char* ssid);

/**
 * @brief Get current connection status
 * @param handle Tool handle
 * @return true if connected, false otherwise
 */
bool network_tool_is_connected(network_tool_handle_t handle);

/**
 * @brief Get current IP address (STA mode)
 * @param handle Tool handle
 * @param ip_str Buffer for IP address string
 * @param len Buffer length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_get_ip_address(network_tool_handle_t handle, char* ip_str, size_t len);

// =============================================================================
// Configuration Management Interface
// =============================================================================

/**
 * @brief Load configuration from file
 * @param handle Tool handle
 * @param file_path Path to configuration file
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_load_config(network_tool_handle_t handle, const char* file_path);

/**
 * @brief Save configuration to file
 * @param handle Tool handle
 * @param file_path Path to configuration file (NULL for default)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_save_config(network_tool_handle_t handle, const char* file_path);

/**
 * @brief Update tool configuration at runtime
 * @param handle Tool handle
 * @param config New configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t network_tool_update_config(network_tool_handle_t handle, const network_tool_config_t *config);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert WiFi tool event type to string
 * @param event_type Event type
 * @return String representation
 */
const char* network_tool_event_to_string(network_tool_event_type_t event_type);

/**
 * @brief Convert WiFi auth mode to string
 * @param auth_mode Authentication mode
 * @return String representation
 */
const char* network_tool_auth_mode_to_string(wifi_auth_mode_t auth_mode);

/**
 * @brief Get WiFi error reason string
 * @param reason Error reason code
 * @return String representation
 */
const char* network_tool_error_to_string(wifi_err_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_TOOL_H