/**
 * @file debug_tool.h
 * @brief MCP-Inspired Debug Tool - ASCII Dashboard and System Monitoring
 * 
 * Centralized debug and monitoring tool extracted from feedback_tool.
 * Provides ASCII dashboard and system status aggregation per process map authority.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// MCP-Inspired Tool Metadata
// =============================================================================

/**
 * @brief Tool identification and capabilities
 */
#define DEBUG_TOOL_ID "debug"
#define DEBUG_TOOL_VERSION "1.0.0"
#define DEBUG_TOOL_DESCRIPTION "ASCII dashboard and system monitoring hub"

/**
 * @brief Tool capabilities bitmask
 */
typedef enum {
    DEBUG_CAP_ASCII_DASHBOARD   = (1 << 0),  // ASCII art dashboard generation
    DEBUG_CAP_JSON_STATUS       = (1 << 1),  // JSON status generation
    DEBUG_CAP_TOOL_MONITORING   = (1 << 2),  // Multi-tool status aggregation
    DEBUG_CAP_ROBOT_EXPRESSIONS = (1 << 3),  // Emotional status indicators
    DEBUG_CAP_BUFFER_VISUAL     = (1 << 4),  // Buffer visualization
    DEBUG_CAP_REAL_TIME_UPDATE  = (1 << 5),  // Live dashboard updates
    DEBUG_CAP_THREAD_SAFE       = (1 << 6)   // Thread-safe operations
} debug_tool_capabilities_t;

// =============================================================================
// Universal Tool Interface (MCP Pattern)
// =============================================================================

/**
 * @brief Opaque tool handle
 */
typedef struct debug_tool* debug_tool_handle_t;

/**
 * @brief Dashboard configuration structure
 */
typedef struct {
    uint32_t dashboard_buffer_size;      // ASCII dashboard buffer size
    uint32_t json_buffer_size;           // JSON status buffer size
    uint32_t update_interval_ms;         // Dashboard update interval
    bool enable_robot_expressions;       // Enable emotional indicators
    bool enable_buffer_visualization;    // Enable buffer visual status
    bool enable_real_time_updates;       // Enable continuous updates
} debug_tool_config_t;

// =============================================================================
// System Status and Robot Expressions (Process Map Authority)
// =============================================================================

/**
 * @brief Robot expression states for emotional indicators
 */
typedef enum {
    DEBUG_ROBOT_HAPPY       = 0x01,  // (◕‿◕)っ  - System healthy
    DEBUG_ROBOT_FOCUSED     = 0x02,  // (•ᴗ•)っ  - System working
    DEBUG_ROBOT_CONCERNED   = 0x03,  // (ಠ_ಠ)っ  - System warning
    DEBUG_ROBOT_ANGRY       = 0x04,  // (╯°□°)╯ - System error
    DEBUG_ROBOT_NEUTRAL     = 0x05   // (•_•)っ  - System idle
} debug_robot_expression_t;

/**
 * @brief Buffer visualization modes
 */
typedef enum {
    DEBUG_BUFFER_DOTS       = 0x01,  // ●●●○○...○  - Dot visualization
    DEBUG_BUFFER_BARS       = 0x02,  // [███░░░░░░] - Bar visualization
    DEBUG_BUFFER_NUMERIC    = 0x03   // 15/50       - Numeric display
} debug_buffer_visual_mode_t;

/**
 * @brief System health status
 */
typedef enum {
    DEBUG_HEALTH_EXCELLENT  = 0x01,  // All systems green
    DEBUG_HEALTH_GOOD       = 0x02,  // Minor warnings
    DEBUG_HEALTH_WARNING    = 0x03,  // Some issues detected
    DEBUG_HEALTH_CRITICAL   = 0x04,  // Major problems
    DEBUG_HEALTH_UNKNOWN    = 0x05   // Status unavailable
} debug_system_health_t;

// =============================================================================
// Dashboard and Status Structures
// =============================================================================

/**
 * @brief ASCII dashboard result
 */
typedef struct {
    char* ascii_dashboard;               // ASCII art dashboard string
    size_t dashboard_length;             // Dashboard string length
    debug_robot_expression_t robot_expr; // Current robot expression
    debug_system_health_t system_health; // Overall system health
    uint32_t update_count;               // Dashboard update counter
    bool generation_success;             // Dashboard generation status
} debug_dashboard_result_t;

/**
 * @brief JSON status result
 */
typedef struct {
    char* json_status;                   // JSON status string
    size_t json_length;                  // JSON string length
    uint32_t tools_monitored;            // Number of tools monitored
    bool generation_success;             // JSON generation status
} debug_json_status_result_t;

/**
 * @brief Tool status structure
 */
typedef struct {
    bool is_initialized;                 // Tool initialization status
    uint32_t dashboards_generated;       // Total dashboards generated
    uint32_t json_reports_generated;     // Total JSON reports generated
    uint32_t tools_registered;           // Number of registered tools
    debug_system_health_t system_health; // Current system health
    uint32_t last_error_code;            // Last error encountered
} debug_tool_status_t;

// =============================================================================
// Tool Handle Registration (For Status Collection)
// =============================================================================

/**
 * @brief Tool handle information for registration
 */
typedef struct {
    void* tool_handle;                   // Opaque tool handle
    const char* tool_id;                 // Tool identifier
    const char* tool_version;            // Tool version
    esp_err_t (*get_status_func)(void* handle, void* status);  // Status function
    size_t status_struct_size;           // Size of status structure
} debug_tool_registration_t;

// =============================================================================
// MCP Tool Interface Functions
// =============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
debug_tool_config_t debug_tool_create_default_config(void);

/**
 * @brief Initialize debug tool
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
debug_tool_handle_t debug_tool_init(const debug_tool_config_t* config);

/**
 * @brief Cleanup debug tool
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t debug_tool_cleanup(debug_tool_handle_t handle);

/**
 * @brief Get tool identification
 * @return Tool ID string
 */
const char* debug_tool_get_id(void);

/**
 * @brief Get tool version
 * @return Tool version string
 */
const char* debug_tool_get_version(void);

/**
 * @brief Get tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
debug_tool_capabilities_t debug_tool_get_capabilities(debug_tool_handle_t handle);

/**
 * @brief Get tool status
 * @param handle Tool handle
 * @param status Status structure to fill
 * @return ESP_OK on success
 */
esp_err_t debug_tool_get_status(debug_tool_handle_t handle, debug_tool_status_t* status);

// =============================================================================
// Tool Registration Functions (For Status Collection)
// =============================================================================

/**
 * @brief Register tool for monitoring
 * @param handle Debug tool handle
 * @param registration Tool registration information
 * @return ESP_OK on success
 */
esp_err_t debug_tool_register_tool(debug_tool_handle_t handle,
                                 const debug_tool_registration_t* registration);

/**
 * @brief Unregister tool from monitoring
 * @param handle Debug tool handle
 * @param tool_id Tool identifier to unregister
 * @return ESP_OK on success
 */
esp_err_t debug_tool_unregister_tool(debug_tool_handle_t handle, const char* tool_id);

// =============================================================================
// ASCII Dashboard Functions (Process Map Authority)
// =============================================================================

/**
 * @brief Generate ASCII dashboard (main function per process maps)
 * @param handle Debug tool handle
 * @param result Dashboard result structure
 * @return ESP_OK on success
 */
esp_err_t debug_tool_generate_dashboard(debug_tool_handle_t handle,
                                      debug_dashboard_result_t* result);

/**
 * @brief Update dashboard with real-time information
 * @param handle Debug tool handle
 * @return ESP_OK on success
 */
esp_err_t debug_tool_update_dashboard(debug_tool_handle_t handle);

/**
 * @brief Free dashboard result
 * @param result Result structure to free
 */
void debug_tool_free_dashboard_result(debug_dashboard_result_t* result);

// =============================================================================
// JSON Status Functions
// =============================================================================

/**
 * @brief Generate JSON system status
 * @param handle Debug tool handle
 * @param result JSON status result structure
 * @return ESP_OK on success
 */
esp_err_t debug_tool_generate_json_status(debug_tool_handle_t handle,
                                        debug_json_status_result_t* result);

/**
 * @brief Free JSON status result
 * @param result Result structure to free
 */
void debug_tool_free_json_result(debug_json_status_result_t* result);

// =============================================================================
// System Health and Expression Functions (Process Map Requirements)
// =============================================================================

/**
 * @brief Set robot expression based on system state
 * @param handle Debug tool handle
 * @param expression Robot expression to set
 * @return ESP_OK on success
 */
esp_err_t debug_tool_set_robot_expression(debug_tool_handle_t handle,
                                        debug_robot_expression_t expression);

/**
 * @brief Update buffer visualization
 * @param handle Debug tool handle
 * @param used_slots Number of used buffer slots
 * @param total_slots Total buffer slots available
 * @param mode Visualization mode
 * @return ESP_OK on success
 */
esp_err_t debug_tool_update_buffer_visualization(debug_tool_handle_t handle,
                                               uint32_t used_slots,
                                               uint32_t total_slots,
                                               debug_buffer_visual_mode_t mode);

/**
 * @brief Increment retry counter visualization
 * @param handle Debug tool handle
 * @param current_retry Current retry attempt
 * @param max_retries Maximum retry attempts
 * @return ESP_OK on success
 */
esp_err_t debug_tool_increment_retry_counter(debug_tool_handle_t handle,
                                           uint32_t current_retry,
                                           uint32_t max_retries);

/**
 * @brief Set offline/online mode indicator
 * @param handle Debug tool handle
 * @param is_offline Offline mode status
 * @return ESP_OK on success
 */
esp_err_t debug_tool_set_offline_mode(debug_tool_handle_t handle, bool is_offline);

/**
 * @brief Calculate overall system health from registered tools
 * @param handle Debug tool handle
 * @param health System health output
 * @return ESP_OK on success
 */
esp_err_t debug_tool_calculate_system_health(debug_tool_handle_t handle,
                                            debug_system_health_t* health);

#ifdef __cplusplus
}
#endif