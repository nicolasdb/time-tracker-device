/**
 * @file system_monitor_tool.h
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
#define SYSTEM_MONITOR_TOOL_ID "system_monitor"
#define SYSTEM_MONITOR_TOOL_VERSION "1.0.0"
#define SYSTEM_MONITOR_TOOL_DESCRIPTION "ASCII dashboard and system monitoring hub"

/**
 * @brief Tool capabilities bitmask
 */
typedef enum {
    SYSTEM_MONITOR_CAP_ASCII_DASHBOARD   = (1 << 0),  // ASCII art dashboard generation
    SYSTEM_MONITOR_CAP_JSON_STATUS       = (1 << 1),  // JSON status generation
    SYSTEM_MONITOR_CAP_TOOL_MONITORING   = (1 << 2),  // Multi-tool status aggregation
    SYSTEM_MONITOR_CAP_ROBOT_EXPRESSIONS = (1 << 3),  // Emotional status indicators
    SYSTEM_MONITOR_CAP_BUFFER_VISUAL     = (1 << 4),  // Buffer visualization
    SYSTEM_MONITOR_CAP_REAL_TIME_UPDATE  = (1 << 5),  // Live dashboard updates
    SYSTEM_MONITOR_CAP_THREAD_SAFE       = (1 << 6)   // Thread-safe operations
} system_monitor_tool_capabilities_t;

// =============================================================================
// Universal Tool Interface (MCP Pattern)
// =============================================================================

/**
 * @brief Opaque tool handle
 */
typedef struct system_monitor_tool* system_monitor_tool_handle_t;

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
} system_monitor_tool_config_t;

// =============================================================================
// System Status and Robot Expressions (Process Map Authority)
// =============================================================================

/**
 * @brief Robot expression states for emotional indicators
 */
typedef enum {
    SYSTEM_MONITOR_ROBOT_HAPPY       = 0x01,  // (◕‿◕)っ  - System healthy
    SYSTEM_MONITOR_ROBOT_FOCUSED     = 0x02,  // (•ᴗ•)っ  - System working
    SYSTEM_MONITOR_ROBOT_CONCERNED   = 0x03,  // (ಠ_ಠ)っ  - System warning
    SYSTEM_MONITOR_ROBOT_ANGRY       = 0x04,  // (╯°□°)╯ - System error
    SYSTEM_MONITOR_ROBOT_NEUTRAL     = 0x05   // (•_•)っ  - System idle
} system_monitor_robot_expression_t;

/**
 * @brief Buffer visualization modes
 */
typedef enum {
    SYSTEM_MONITOR_BUFFER_DOTS       = 0x01,  // ●●●○○...○  - Dot visualization
    SYSTEM_MONITOR_BUFFER_BARS       = 0x02,  // [███░░░░░░] - Bar visualization
    SYSTEM_MONITOR_BUFFER_NUMERIC    = 0x03   // 15/50       - Numeric display
} system_monitor_buffer_visual_mode_t;

/**
 * @brief System health status
 */
typedef enum {
    SYSTEM_MONITOR_HEALTH_EXCELLENT  = 0x01,  // All systems green
    SYSTEM_MONITOR_HEALTH_GOOD       = 0x02,  // Minor warnings
    SYSTEM_MONITOR_HEALTH_WARNING    = 0x03,  // Some issues detected
    SYSTEM_MONITOR_HEALTH_CRITICAL   = 0x04,  // Major problems
    SYSTEM_MONITOR_HEALTH_UNKNOWN    = 0x05   // Status unavailable
} system_monitor_system_health_t;

// =============================================================================
// Dashboard and Status Structures
// =============================================================================

/**
 * @brief ASCII dashboard result
 */
typedef struct {
    char* ascii_dashboard;               // ASCII art dashboard string
    size_t dashboard_length;             // Dashboard string length
    system_monitor_robot_expression_t robot_expr; // Current robot expression
    system_monitor_system_health_t system_health; // Overall system health
    uint32_t update_count;               // Dashboard update counter
    bool generation_success;             // Dashboard generation status
} system_monitor_dashboard_result_t;

/**
 * @brief JSON status result
 */
typedef struct {
    char* json_status;                   // JSON status string
    size_t json_length;                  // JSON string length
    uint32_t tools_monitored;            // Number of tools monitored
    bool generation_success;             // JSON generation status
} system_monitor_json_status_result_t;

/**
 * @brief Tool status structure
 */
typedef struct {
    bool is_initialized;                 // Tool initialization status
    uint32_t dashboards_generated;       // Total dashboards generated
    uint32_t json_reports_generated;     // Total JSON reports generated
    uint32_t tools_registered;           // Number of registered tools
    system_monitor_system_health_t system_health; // Current system health
    uint32_t last_error_code;            // Last error encountered
} system_monitor_tool_status_t;

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
} system_monitor_tool_registration_t;

// =============================================================================
// MCP Tool Interface Functions
// =============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
system_monitor_tool_config_t system_monitor_tool_create_default_config(void);

/**
 * @brief Initialize debug tool
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
system_monitor_tool_handle_t system_monitor_tool_init(const system_monitor_tool_config_t* config);

/**
 * @brief Cleanup debug tool
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_cleanup(system_monitor_tool_handle_t handle);

/**
 * @brief Get tool identification
 * @return Tool ID string
 */
const char* system_monitor_tool_get_id(void);

/**
 * @brief Get tool version
 * @return Tool version string
 */
const char* system_monitor_tool_get_version(void);

/**
 * @brief Get tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
system_monitor_tool_capabilities_t system_monitor_tool_get_capabilities(system_monitor_tool_handle_t handle);

/**
 * @brief Get tool status
 * @param handle Tool handle
 * @param status Status structure to fill
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_get_status(system_monitor_tool_handle_t handle, system_monitor_tool_status_t* status);

// =============================================================================
// Tool Registration Functions (For Status Collection)
// =============================================================================

/**
 * @brief Register tool for monitoring
 * @param handle Debug tool handle
 * @param registration Tool registration information
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_register_tool(system_monitor_tool_handle_t handle,
                                 const system_monitor_tool_registration_t* registration);

/**
 * @brief Unregister tool from monitoring
 * @param handle Debug tool handle
 * @param tool_id Tool identifier to unregister
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_unregister_tool(system_monitor_tool_handle_t handle, const char* tool_id);

// =============================================================================
// ASCII Dashboard Functions (Process Map Authority)
// =============================================================================

/**
 * @brief Generate ASCII dashboard (main function per process maps)
 * @param handle Debug tool handle
 * @param result Dashboard result structure
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_generate_dashboard(system_monitor_tool_handle_t handle,
                                      system_monitor_dashboard_result_t* result);

/**
 * @brief Update dashboard with real-time information
 * @param handle Debug tool handle
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_update_dashboard(system_monitor_tool_handle_t handle);

/**
 * @brief Free dashboard result
 * @param result Result structure to free
 */
void system_monitor_tool_free_dashboard_result(system_monitor_dashboard_result_t* result);

// =============================================================================
// JSON Status Functions
// =============================================================================

/**
 * @brief Generate JSON system status
 * @param handle Debug tool handle
 * @param result JSON status result structure
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_generate_json_status(system_monitor_tool_handle_t handle,
                                        system_monitor_json_status_result_t* result);

/**
 * @brief Free JSON status result
 * @param result Result structure to free
 */
void system_monitor_tool_free_json_result(system_monitor_json_status_result_t* result);

// =============================================================================
// System Health and Expression Functions (Process Map Requirements)
// =============================================================================

/**
 * @brief Set robot expression based on system state
 * @param handle Debug tool handle
 * @param expression Robot expression to set
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_set_robot_expression(system_monitor_tool_handle_t handle,
                                        system_monitor_robot_expression_t expression);

/**
 * @brief Update buffer visualization
 * @param handle Debug tool handle
 * @param used_slots Number of used buffer slots
 * @param total_slots Total buffer slots available
 * @param mode Visualization mode
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_update_buffer_visualization(system_monitor_tool_handle_t handle,
                                               uint32_t used_slots,
                                               uint32_t total_slots,
                                               system_monitor_buffer_visual_mode_t mode);

/**
 * @brief Increment retry counter visualization
 * @param handle Debug tool handle
 * @param current_retry Current retry attempt
 * @param max_retries Maximum retry attempts
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_increment_retry_counter(system_monitor_tool_handle_t handle,
                                           uint32_t current_retry,
                                           uint32_t max_retries);

/**
 * @brief Set offline/online mode indicator
 * @param handle Debug tool handle
 * @param is_offline Offline mode status
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_set_offline_mode(system_monitor_tool_handle_t handle, bool is_offline);

/**
 * @brief Calculate overall system health from registered tools
 * @param handle Debug tool handle
 * @param health System health output
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_calculate_system_health(system_monitor_tool_handle_t handle,
                                            system_monitor_system_health_t* health);

// =============================================================================
// Event-Driven Session Timing (Process Map Authority: Constitutional Requirement)
// =============================================================================

/**
 * @brief Session timing context structure
 */
typedef struct {
    uint64_t session_start_time_ms;    // Session start timestamp
    bool session_active;               // Current session state
    uint32_t session_duration_min;     // Current session duration in minutes
    bool flow_awareness_triggered;     // 60-minute notification sent
    bool flow_urgency_triggered;       // 90-minute notification sent
    char current_tag_uid[32];          // Active tag UID
} session_timing_context_t;

/**
 * @brief Initialize system monitor with event-driven architecture
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_start_event_subscription(system_monitor_tool_handle_t handle);

/**
 * @brief Stop event subscription and cleanup
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_stop_event_subscription(system_monitor_tool_handle_t handle);

/**
 * @brief Update session timing and check for flow awareness triggers
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_update_session_timing(system_monitor_tool_handle_t handle);

/**
 * @brief Get current session timing context
 * @param handle Tool handle
 * @param context Output session context
 * @return ESP_OK on success
 */
esp_err_t system_monitor_tool_get_session_context(system_monitor_tool_handle_t handle,
                                                  session_timing_context_t* context);

#ifdef __cplusplus
}
#endif