/**
 * @file network_tool.h
 * @brief Constitutional Network Tool Interface - WiFi Connectivity
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Network connectivity foundation for ecosystem Layer 1
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include <inttypes.h>
#include "fs_tool.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constitutional Network Tool
// =============================================================================

/**
 * @brief Constitutional network tool handle
 */
typedef struct network_tool* network_tool_handle_t;

/**
 * @brief Network connection state
 */
typedef enum {
    NETWORK_STATE_DISCONNECTED,
    NETWORK_STATE_CONNECTING,
    NETWORK_STATE_CONNECTED,
    NETWORK_STATE_FAILED,
    NETWORK_STATE_AP_MODE
} network_state_t;

/**
 * @brief Network tool configuration
 */
typedef struct {
    char config_file[64];           // Path to wifi.json configuration
    uint32_t connection_timeout_ms; // Connection timeout
    uint32_t retry_attempts;        // Max retry attempts
    uint32_t retry_delay_ms;        // Delay between retries
    bool enable_ap_fallback;        // Enable AP mode fallback
    bool publish_events;            // Publish ESP_EVENT messages
} network_tool_config_t;

/**
 * @brief Network tool status
 */
typedef struct {
    bool is_initialized;
    bool is_active;
    network_state_t state;
    bool wifi_hardware_ok;
    char connected_ssid[32];
    char ip_address[16];
    int8_t rssi;
    uint32_t connection_count;
    uint32_t error_count;
    uint64_t last_connect_time_us;
} network_tool_status_t;

// =============================================================================
// Constitutional Network Events
// =============================================================================

ESP_EVENT_DECLARE_BASE(NETWORK_TOOL_EVENTS);

typedef enum {
    NETWORK_TOOL_EVENT_CONNECTING = 0,
    NETWORK_TOOL_EVENT_CONNECTED,
    NETWORK_TOOL_EVENT_DISCONNECTED,
    NETWORK_TOOL_EVENT_FAILED,
    NETWORK_TOOL_EVENT_IP_ACQUIRED,
    NETWORK_TOOL_EVENT_AP_STARTED,
    NETWORK_TOOL_EVENT_CONFIG_LOADED
} network_tool_event_id_t;

typedef struct {
    network_state_t state;
    char ssid[32];
    char ip_address[16];
    int8_t rssi;
    uint64_t timestamp_us;
} network_tool_event_t;

// =============================================================================
// Constitutional Network Interface
// =============================================================================

/**
 * @brief Get constitutional network tool identification
 */
const char* network_tool_get_id(void);

/**
 * @brief Get constitutional network tool version
 */
const char* network_tool_get_version(void);

/**
 * @brief Create default network configuration
 */
network_tool_config_t network_tool_create_default_config(void);

/**
 * @brief Initialize constitutional network tool
 */
network_tool_handle_t network_tool_init(const network_tool_config_t *config);

/**
 * @brief Deinitialize constitutional network tool
 */
esp_err_t network_tool_deinit(network_tool_handle_t handle);

/**
 * @brief Get constitutional network tool status
 */
esp_err_t network_tool_get_status(network_tool_handle_t handle, network_tool_status_t *status);

/**
 * @brief Set fs_tool dependency for configuration loading
 */
esp_err_t network_tool_set_fs_dependency(network_tool_handle_t handle, fs_tool_handle_t fs_tool);

/**
 * @brief Connect to WiFi network using configuration
 */
esp_err_t network_tool_connect(network_tool_handle_t handle);

/**
 * @brief Disconnect from WiFi network
 */
esp_err_t network_tool_disconnect(network_tool_handle_t handle);

/**
 * @brief Start AP mode (fallback)
 */
esp_err_t network_tool_start_ap(network_tool_handle_t handle);

/**
 * @brief Stop AP mode
 */
esp_err_t network_tool_stop_ap(network_tool_handle_t handle);

/**
 * @brief Perform WiFi hardware self-test
 */
esp_err_t network_tool_hardware_self_test(network_tool_handle_t handle);

/**
 * @brief Scan for available networks
 */
esp_err_t network_tool_scan_networks(network_tool_handle_t handle);

#ifdef __cplusplus
}
#endif

/**
 * @brief Constitutional Network Tool Example
 * 
 * // Initialize network tool
 * network_tool_config_t config = network_tool_create_default_config();
 * network_tool_handle_t network = network_tool_init(&config);
 * 
 * // Set fs_tool dependency for configuration loading
 * network_tool_set_fs_dependency(network, fs_tool);
 * 
 * // Connect to WiFi using wifi.json configuration
 * network_tool_connect(network);
 * 
 * // Get status
 * network_tool_status_t status;
 * network_tool_get_status(network, &status);
 * 
 * // Cleanup
 * network_tool_deinit(network);
 */