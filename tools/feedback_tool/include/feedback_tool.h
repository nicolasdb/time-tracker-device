/**
 * @file feedback_tool.h
 * @brief MCP-Inspired Feedback Tool - Visual State Management
 * 
 * Universal tool interface for visual feedback with priority-based state management.
 * Transformed from feedback_manager to follow MCP tool composition patterns.
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
#define FEEDBACK_TOOL_ID "feedback"
#define FEEDBACK_TOOL_VERSION "1.0.0"
#define FEEDBACK_TOOL_DESCRIPTION "Visual state feedback with priority queue management"

/**
 * @brief Tool capabilities bitmask
 */
typedef enum {
    FEEDBACK_CAP_LED_CONTROL     = (1 << 0),  // RGB LED control
    FEEDBACK_CAP_PRIORITY_QUEUE  = (1 << 1),  // Priority-based state management
    FEEDBACK_CAP_ANIMATIONS      = (1 << 2),  // Complex animation patterns
    FEEDBACK_CAP_AUTO_EXPIRE     = (1 << 3),  // Automatic state expiration
    FEEDBACK_CAP_THREAD_SAFE     = (1 << 4)   // Thread-safe operations
} feedback_tool_capabilities_t;

// =============================================================================
// Universal Tool Interface (MCP Pattern)
// =============================================================================

/**
 * @brief Opaque tool handle (preserves existing handle pattern)
 */
typedef struct feedback_tool* feedback_tool_handle_t;

/**
 * @brief Tool configuration structure
 */
typedef struct {
    uint8_t led_gpio;                    // GPIO pin for RGB LED
    uint8_t max_brightness;              // Maximum LED brightness (0-255)
    uint32_t breathing_period_ms;        // Breathing animation period
    bool auto_cleanup_enabled;           // Enable automatic state cleanup
    uint32_t cleanup_interval_ms;        // State cleanup check interval
} feedback_tool_config_t;

// =============================================================================
// System State Definitions (Enhanced from Original)
// =============================================================================

/**
 * @brief Universal system state enum for feedback
 * Enhanced with MCP-inspired categorization
 */
typedef enum {
    // System Core States
    FEEDBACK_STATE_BOOTING           = 0x0000,
    FEEDBACK_STATE_IDLE              = 0x0001,
    FEEDBACK_STATE_ERROR             = 0x0002,
    FEEDBACK_STATE_SHUTDOWN          = 0x0003,
    
    // WiFi Tool States
    FEEDBACK_STATE_WIFI_CONNECTING   = 0x0100,
    FEEDBACK_STATE_WIFI_CONNECTED    = 0x0101,
    FEEDBACK_STATE_WIFI_FAILED       = 0x0102,
    FEEDBACK_STATE_WIFI_AP_MODE      = 0x0103,
    
    // Time Tool States  
    FEEDBACK_STATE_TIME_SYNCING      = 0x0200,
    FEEDBACK_STATE_TIME_SYNCED       = 0x0201,
    FEEDBACK_STATE_TIME_SYNC_FAILED  = 0x0202,
    
    // RFID Tool States
    FEEDBACK_STATE_RFID_INITIALIZING = 0x0300,
    FEEDBACK_STATE_RFID_ACTIVE       = 0x0301,
    FEEDBACK_STATE_RFID_ERROR        = 0x0302,
    FEEDBACK_STATE_TAG_DETECTED      = 0x0303,
    FEEDBACK_STATE_TAG_READ_ERROR    = 0x0304,
    FEEDBACK_STATE_TAG_IGNORED       = 0x0305,  // Process Map Authority: yellow flash for ignored duplicates
    
    // Webhook Tool States
    FEEDBACK_STATE_WEBHOOK_SENDING   = 0x0400,
    FEEDBACK_STATE_WEBHOOK_SUCCESS   = 0x0401,
    FEEDBACK_STATE_WEBHOOK_ERROR     = 0x0402,
    FEEDBACK_STATE_WEBHOOK_QUEUED    = 0x0403,
    
    // Webserver Tool States
    FEEDBACK_STATE_WEBSERVER_STARTING = 0x0500,
    FEEDBACK_STATE_WEBSERVER_ACTIVE   = 0x0501,
    FEEDBACK_STATE_WEBSERVER_ERROR    = 0x0502,
    
    // Initialization Sequence States
    FEEDBACK_STATE_INIT_START        = 0x1000,
    FEEDBACK_STATE_INIT_FS           = 0x1001,
    FEEDBACK_STATE_INIT_WIFI_PREP    = 0x1002,
    FEEDBACK_STATE_INIT_TIME         = 0x1003,
    FEEDBACK_STATE_INIT_WEBHOOK      = 0x1004,
    FEEDBACK_STATE_INIT_RFID         = 0x1005,
    FEEDBACK_STATE_INIT_COMPLETE     = 0x1006,
    
    // Flow Awareness States (Process Map Authority: Constitutional Requirement)
    FEEDBACK_STATE_FLOW_AWARENESS    = 0x2100,  // Orange breathing at 60min sessions
    FEEDBACK_STATE_FLOW_URGENCY      = 0x2101,  // Orange pulsing at 90min sessions
    
    // Tool Communication States
    FEEDBACK_STATE_TOOL_REGISTERED   = 0x2000,
    FEEDBACK_STATE_TOOL_ERROR        = 0x2001,
    FEEDBACK_STATE_TOOL_DISCONNECTED = 0x2002,
    
    // Must be last
    FEEDBACK_STATE_MAX = 0xFFFF
} feedback_state_t;

/**
 * @brief State priority levels for queue management
 */
typedef enum {
    FEEDBACK_PRIORITY_LOW = 0,      // Ambient states (idle breathing)
    FEEDBACK_PRIORITY_MEDIUM = 1,   // Status updates (WiFi, connections)
    FEEDBACK_PRIORITY_HIGH = 2,     // Critical events (tag detected, errors)
    FEEDBACK_PRIORITY_CRITICAL = 3  // System errors, tool failures
} feedback_priority_t;

/**
 * @brief Tool status structure
 */
typedef struct {
    bool is_initialized;                 // Tool initialization state
    bool is_active;                      // Tool active state
    uint8_t queue_count;                 // Number of queued states
    feedback_state_t current_state;      // Currently active state
    uint32_t uptime_ms;                  // Tool uptime in milliseconds
    feedback_tool_capabilities_t capabilities; // Tool capabilities
} feedback_tool_status_t;

// =============================================================================
// MCP Tool Interface Functions
// =============================================================================

/**
 * @brief Initialize feedback tool with configuration
 * @param config Tool configuration
 * @return Tool handle on success, NULL on failure
 */
feedback_tool_handle_t feedback_tool_init(const feedback_tool_config_t *config);

/**
 * @brief Deinitialize feedback tool and cleanup resources
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_deinit(feedback_tool_handle_t handle);

/**
 * @brief Get tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
feedback_tool_capabilities_t feedback_tool_get_capabilities(feedback_tool_handle_t handle);

/**
 * @brief Get tool status
 * @param handle Tool handle
 * @param status Pointer to store status information
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_get_status(feedback_tool_handle_t handle, feedback_tool_status_t *status);

/**
 * @brief Get tool identification string
 * @return Static tool ID string
 */
const char* feedback_tool_get_id(void);

/**
 * @brief Get tool version string
 * @return Static version string
 */
const char* feedback_tool_get_version(void);

// =============================================================================
// State Management Interface (Enhanced from Original)
// =============================================================================

/**
 * @brief Set primary system state with priority and duration
 * @param handle Tool handle
 * @param state System state to set
 * @param priority State priority level
 * @param duration_ms Duration in milliseconds (0 = permanent)
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_set_state(feedback_tool_handle_t handle, 
                                  feedback_state_t state,
                                  feedback_priority_t priority,
                                  uint32_t duration_ms);

/**
 * @brief Set state with default priority (inferred from state type)
 * @param handle Tool handle
 * @param state System state to set
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_set_state_simple(feedback_tool_handle_t handle, feedback_state_t state);

/**
 * @brief Flash a temporary state indication
 * @param handle Tool handle
 * @param state State to flash
 * @param count Number of times to flash
 * @param flash_duration_ms Duration of each flash
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_flash_event(feedback_tool_handle_t handle, 
                                    feedback_state_t state, 
                                    int count,
                                    uint32_t flash_duration_ms);

/**
 * @brief Clear specific state from queue
 * @param handle Tool handle
 * @param state State to clear
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_clear_state(feedback_tool_handle_t handle, feedback_state_t state);

/**
 * @brief Clear all states and reset to idle
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_reset(feedback_tool_handle_t handle);

/**
 * @brief Get current active state
 * @param handle Tool handle
 * @return Current active state
 */
feedback_state_t feedback_tool_get_current_state(feedback_tool_handle_t handle);

/**
 * @brief Get number of queued states
 * @param handle Tool handle
 * @return Number of states in queue
 */
uint8_t feedback_tool_get_queue_count(feedback_tool_handle_t handle);

// =============================================================================
// MCP Tool Registry Integration (Phase 2 target)
// =============================================================================

/**
 * @brief Tool registry entry structure
 */
typedef struct {
    const char* tool_id;
    const char* version;
    const char* description;
    feedback_tool_capabilities_t capabilities;
    feedback_tool_handle_t (*init_func)(const feedback_tool_config_t*);
    esp_err_t (*deinit_func)(feedback_tool_handle_t);
} feedback_tool_registry_t;

/**
 * @brief Get tool registry entry (for tool discovery)
 * @return Pointer to static registry entry
 */
const feedback_tool_registry_t* feedback_tool_get_registry_entry(void);

// =============================================================================
// Utility Functions (Enhanced from Original)
// =============================================================================

/**
 * @brief Validate initialization step with visual feedback
 * @param handle Tool handle
 * @param step_name Name of initialization step (for logging)
 * @param success Whether the step was successful
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_validate_init_step(feedback_tool_handle_t handle, 
                                           const char* step_name,
                                           bool success);

/**
 * @brief Convert state enum to human-readable string
 * @param state State to convert
 * @return Static string representation
 */
const char* feedback_tool_state_to_string(feedback_state_t state);

/**
 * @brief Convert priority enum to human-readable string
 * @param priority Priority to convert
 * @return Static string representation
 */
const char* feedback_tool_priority_to_string(feedback_priority_t priority);

/**
 * @brief Create default tool configuration
 * @return Default configuration structure
 */
feedback_tool_config_t feedback_tool_create_default_config(void);

// =============================================================================
// Dashboard & Status Aggregation (Phase 4.3 Enhancement)
// =============================================================================

/**
 * @brief System dashboard data structure
 */
typedef struct {
    char ascii_dashboard[512];           // ASCII formatted dashboard (reduced)
    char json_status[256];               // JSON structured status (reduced)
    uint32_t timestamp;                  // Dashboard generation timestamp
    bool is_operational;                 // Overall system health
} feedback_dashboard_t;

/**
 * @brief Generate ASCII dashboard with system status
 * @param handle Tool handle
 * @param dashboard Output dashboard structure
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_generate_dashboard(feedback_tool_handle_t handle, feedback_dashboard_t* dashboard);

/**
 * @brief Subscribe to tool events for status aggregation
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_subscribe_to_all_events(feedback_tool_handle_t handle);

/**
 * @brief Get latest system status as JSON string
 * @param handle Tool handle
 * @param json_buffer Output buffer for JSON
 * @param buffer_size Size of output buffer
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_get_status_json(feedback_tool_handle_t handle, char* json_buffer, size_t buffer_size);

// =============================================================================
// Flow Awareness Context (Process Map Authority: Constitutional Requirement)
// =============================================================================

/**
 * @brief Set flow awareness context for visual state modification
 * @param handle Tool handle
 * @param flow_active Whether flow awareness should modify visuals
 * @param flow_urgent Whether flow urgency should modify visuals
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_set_flow_context(feedback_tool_handle_t handle, 
                                        bool flow_active, 
                                        bool flow_urgent);

// =============================================================================
// Event-Driven Architecture Functions (Phase 6.0)
// =============================================================================

/**
 * @brief Start event subscription for async visual feedback
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_start_event_subscription(feedback_tool_handle_t handle);

/**
 * @brief Stop event subscription and cleanup
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_stop_event_subscription(feedback_tool_handle_t handle);

#ifdef __cplusplus
}
#endif

/**
 * @brief Tool Usage Example
 * 
 * // Initialize with default config
 * feedback_tool_config_t config = feedback_tool_create_default_config();
 * config.led_gpio = 5;
 * 
 * feedback_tool_handle_t tool = feedback_tool_init(&config);
 * 
 * // Set states with automatic priority inference
 * feedback_tool_set_state_simple(tool, FEEDBACK_STATE_WIFI_CONNECTING);
 * feedback_tool_set_state_simple(tool, FEEDBACK_STATE_WIFI_CONNECTED);
 * 
 * // Set temporary high-priority state
 * feedback_tool_set_state(tool, FEEDBACK_STATE_TAG_DETECTED, 
 *                        FEEDBACK_PRIORITY_HIGH, 0); // Permanent until cleared
 * 
 * // Flash event indication
 * feedback_tool_flash_event(tool, FEEDBACK_STATE_WEBHOOK_SUCCESS, 2, 300);
 * 
 * // Cleanup
 * feedback_tool_clear_state(tool, FEEDBACK_STATE_TAG_DETECTED);
 * feedback_tool_deinit(tool);
 */