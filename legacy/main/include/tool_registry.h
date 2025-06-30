/**
 * @file tool_registry.h
 * @brief MCP-Inspired Tool Registry System
 * 
 * Centralized tool management system to replace global handles.
 * Provides discovery, lifecycle management, and dependency injection.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Tool Registry Configuration
// =============================================================================

#define TOOL_REGISTRY_MAX_TOOLS     16
#define TOOL_REGISTRY_MAX_ID_LENGTH 32
#define TOOL_REGISTRY_MAX_VERSION_LENGTH 16

/**
 * @brief Tool lifecycle states
 */
typedef enum {
    TOOL_STATE_UNINITIALIZED = 0x00,
    TOOL_STATE_INITIALIZING  = 0x01,
    TOOL_STATE_INITIALIZED   = 0x02,
    TOOL_STATE_RUNNING       = 0x03,
    TOOL_STATE_ERROR         = 0x04,
    TOOL_STATE_CLEANUP       = 0x05
} tool_state_t;

/**
 * @brief Tool capabilities bitmask (universal)
 */
typedef uint32_t tool_capabilities_t;

/**
 * @brief Universal tool interface functions
 */
typedef struct {
    const char* (*get_id)(void);
    const char* (*get_version)(void);
    tool_capabilities_t (*get_capabilities)(void* handle);
    esp_err_t (*get_status)(void* handle, void* status);
    esp_err_t (*cleanup)(void* handle);
} tool_interface_t;

/**
 * @brief Tool registration information
 */
typedef struct {
    char tool_id[TOOL_REGISTRY_MAX_ID_LENGTH];
    char tool_version[TOOL_REGISTRY_MAX_VERSION_LENGTH];
    void* tool_handle;
    const tool_interface_t* interface;
    tool_capabilities_t capabilities;
    tool_state_t state;
    uint64_t init_timestamp_us;
    uint32_t error_count;
    bool is_critical;                    // Critical tools required for operation
} tool_registration_entry_t;

/**
 * @brief Tool registry statistics
 */
typedef struct {
    uint32_t total_tools;
    uint32_t initialized_tools;
    uint32_t running_tools;
    uint32_t error_tools;
    uint32_t critical_tools;
    uint64_t registry_init_timestamp_us;
} tool_registry_stats_t;

// =============================================================================
// Tool Registry Interface
// =============================================================================

/**
 * @brief Initialize tool registry
 * @return ESP_OK on success
 */
esp_err_t tool_registry_init(void);

/**
 * @brief Cleanup tool registry
 * @return ESP_OK on success
 */
esp_err_t tool_registry_cleanup(void);

/**
 * @brief Register tool with registry
 * @param tool_id Tool identifier
 * @param tool_handle Tool handle
 * @param interface Tool interface functions
 * @param is_critical Whether tool is critical for operation
 * @return ESP_OK on success
 */
esp_err_t tool_registry_register(const char* tool_id,
                                void* tool_handle,
                                const tool_interface_t* interface,
                                bool is_critical);

/**
 * @brief Unregister tool from registry
 * @param tool_id Tool identifier
 * @return ESP_OK on success
 */
esp_err_t tool_registry_unregister(const char* tool_id);

/**
 * @brief Get tool handle by ID
 * @param tool_id Tool identifier
 * @param tool_handle Output tool handle
 * @return ESP_OK on success
 */
esp_err_t tool_registry_get_handle(const char* tool_id, void** tool_handle);

/**
 * @brief Get tool state by ID
 * @param tool_id Tool identifier
 * @param state Output tool state
 * @return ESP_OK on success
 */
esp_err_t tool_registry_get_state(const char* tool_id, tool_state_t* state);

/**
 * @brief Set tool state by ID
 * @param tool_id Tool identifier
 * @param state New tool state
 * @return ESP_OK on success
 */
esp_err_t tool_registry_set_state(const char* tool_id, tool_state_t state);

/**
 * @brief Get tool capabilities by ID
 * @param tool_id Tool identifier
 * @param capabilities Output capabilities
 * @return ESP_OK on success
 */
esp_err_t tool_registry_get_capabilities(const char* tool_id, tool_capabilities_t* capabilities);

/**
 * @brief Get registry statistics
 * @param stats Output statistics structure
 * @return ESP_OK on success
 */
esp_err_t tool_registry_get_stats(tool_registry_stats_t* stats);

/**
 * @brief Check if all critical tools are running
 * @param all_critical_running Output boolean
 * @return ESP_OK on success
 */
esp_err_t tool_registry_check_critical_tools(bool* all_critical_running);

/**
 * @brief Get list of all registered tool IDs
 * @param tool_ids Array of tool ID strings (must be pre-allocated)
 * @param max_tools Maximum number of tools to return
 * @param actual_count Actual number of tools returned
 * @return ESP_OK on success
 */
esp_err_t tool_registry_list_tools(char tool_ids[][TOOL_REGISTRY_MAX_ID_LENGTH],
                                  uint32_t max_tools,
                                  uint32_t* actual_count);

/**
 * @brief Increment error count for tool
 * @param tool_id Tool identifier
 * @return ESP_OK on success
 */
esp_err_t tool_registry_increment_error_count(const char* tool_id);

/**
 * @brief Get tool registration entry (for debug purposes)
 * @param tool_id Tool identifier
 * @param entry Output registration entry
 * @return ESP_OK on success
 */
esp_err_t tool_registry_get_entry(const char* tool_id, tool_registration_entry_t* entry);

// =============================================================================
// Process Map Boot Sequence Support
// =============================================================================

/**
 * @brief Tool initialization order per process maps
 */
typedef enum {
    TOOL_INIT_ORDER_FS_TOOL       = 1,  // First: File system
    TOOL_INIT_ORDER_SYSTEM_MONITOR_TOOL = 2,  // Second: System Monitor/ASCII dashboard
    TOOL_INIT_ORDER_FEEDBACK_TOOL = 3,  // Third: Visual feedback
    TOOL_INIT_ORDER_NETWORK_TOOL  = 4,  // Fourth: WiFi/network
    TOOL_INIT_ORDER_NTP_TOOL      = 5,  // Fifth: Time synchronization
    TOOL_INIT_ORDER_PAYLOAD_TOOL  = 6,  // Sixth: Payload formatting
    TOOL_INIT_ORDER_RFID_TOOL     = 7,  // Seventh: RFID scanning
    TOOL_INIT_ORDER_HTTP_TOOL     = 8,  // Eighth: HTTP transmission
    TOOL_INIT_ORDER_WEBSERVER_TOOL = 9  // Ninth: AP mode webserver
} tool_init_order_t;

/**
 * @brief Boot sequence tool entry
 */
typedef struct {
    const char* tool_id;
    tool_init_order_t order;
    bool is_initialized;
    esp_err_t init_result;
} boot_sequence_entry_t;

/**
 * @brief Initialize tools in process map order
 * @param sequence Boot sequence entries
 * @param sequence_length Number of entries
 * @return ESP_OK if all tools initialized successfully
 */
esp_err_t tool_registry_boot_sequence_init(boot_sequence_entry_t* sequence,
                                          uint32_t sequence_length);

/**
 * @brief Validate boot sequence completion
 * @param all_initialized Output boolean
 * @return ESP_OK on success
 */
esp_err_t tool_registry_validate_boot_sequence(bool* all_initialized);

#ifdef __cplusplus
}
#endif