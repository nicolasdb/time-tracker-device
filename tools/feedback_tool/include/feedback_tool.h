/**
 * @file feedback_tool.h
 * @brief Constitutional Feedback Tool - LED Visual Feedback Implementation
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Process Map 11 (feedback_fsm) visual feedback management
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constitutional Tool Metadata
// =============================================================================

/**
 * @brief Constitutional tool identification
 */
#define FEEDBACK_TOOL_ID "feedback_tool"
#define FEEDBACK_TOOL_VERSION "6.1.0"
#define FEEDBACK_TOOL_DESCRIPTION "Constitutional LED visual feedback with handle-based design"

/**
 * @brief Constitutional feedback tool capabilities
 */
typedef enum {
    FEEDBACK_TOOL_CAP_LED_CONTROL     = (1 << 0),  // RGB LED control
    FEEDBACK_TOOL_CAP_STATE_MANAGEMENT = (1 << 1),  // Visual state management
    FEEDBACK_TOOL_CAP_EVENT_PUBLISH   = (1 << 2),  // ESP_EVENT publishing
    FEEDBACK_TOOL_CAP_PROCESS_MAP_11  = (1 << 3),  // Process Map 11 compliance
    FEEDBACK_TOOL_CAP_FLOW_AWARENESS  = (1 << 4),  // Flow awareness context
    FEEDBACK_TOOL_CAP_DASHBOARD       = (1 << 5),  // Constitutional dashboard
    FEEDBACK_TOOL_CAP_HEALTH_CHECK    = (1 << 6),  // Health monitoring
    FEEDBACK_TOOL_CAP_ANIMATION       = (1 << 7)   // LED animation patterns
} feedback_tool_capabilities_t;

// =============================================================================
// Constitutional Tool Handle & Configuration
// =============================================================================

/**
 * @brief Opaque constitutional tool handle
 */
typedef struct feedback_tool* feedback_tool_handle_t;

/**
 * @brief Constitutional tool configuration
 */
typedef struct {
    uint8_t led_gpio;                    // GPIO pin for RGB LED
    uint8_t max_brightness;              // Maximum LED brightness (0-255)
    uint32_t breathing_period_ms;        // Breathing animation period
    uint32_t blink_period_ms;           // Blink animation period  
    uint32_t update_interval_ms;        // LED update interval
    bool publish_events;                // Enable ESP_EVENT publishing
    bool enable_flow_awareness;         // Enable flow awareness context
} feedback_tool_config_t;

// =============================================================================
// Constitutional Visual States
// =============================================================================

/**
 * @brief Constitutional visual feedback states per Process Map 11
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
    FEEDBACK_STATE_AP_MODE           = 0x0103,
    
    // RFID Tool States  
    FEEDBACK_STATE_TAG_DETECTED      = 0x0200,
    FEEDBACK_STATE_TAG_IGNORED       = 0x0201,
    FEEDBACK_STATE_RFID_ERROR        = 0x0202,
    
    // NTP Tool States
    FEEDBACK_STATE_NTP_SYNC_STARTED  = 0x0300,
    FEEDBACK_STATE_NTP_SYNCED        = 0x0301,
    FEEDBACK_STATE_NTP_FAILED        = 0x0302,
    
    // HTTP Tool States
    FEEDBACK_STATE_HTTP_SENDING      = 0x0400,
    FEEDBACK_STATE_HTTP_SUCCESS      = 0x0401,
    FEEDBACK_STATE_HTTP_ERROR        = 0x0402,
    
    // Flow Awareness States (Constitutional Requirement)
    FEEDBACK_STATE_FLOW_60           = 0x0500,  // 60-minute flow awareness
    FEEDBACK_STATE_FLOW_90           = 0x0501   // 90-minute flow urgency
} feedback_state_t;

/**
 * @brief Constitutional visual pattern types
 */
typedef enum {
    FEEDBACK_PATTERN_OFF,        // LED off
    FEEDBACK_PATTERN_SOLID,      // Solid color
    FEEDBACK_PATTERN_BREATHING,  // Breathing effect
    FEEDBACK_PATTERN_BLINKING,   // Blinking pattern
    FEEDBACK_PATTERN_PULSING,    // Pulsing effect
    FEEDBACK_PATTERN_SEQUENCE    // Color sequence
} feedback_pattern_t;

// =============================================================================
// Constitutional Status & Events
// =============================================================================

/**
 * @brief Constitutional tool status
 */
typedef struct {
    bool is_initialized;
    bool is_active;
    bool led_hardware_ok;
    feedback_state_t current_state;
    feedback_pattern_t current_pattern;
    uint32_t uptime_ms;
    uint32_t state_changes_count;
    uint32_t error_count;
    uint8_t led_gpio;
    uint8_t current_brightness;
    feedback_tool_capabilities_t capabilities;
    char mount_point[32];            // Future filesystem integration
    char version[16];
} feedback_tool_status_t;

/**
 * @brief Constitutional feedback events
 */
ESP_EVENT_DECLARE_BASE(FEEDBACK_TOOL_EVENTS);

typedef enum {
    FEEDBACK_TOOL_EVENT_STATE_CHANGED = 0,
    FEEDBACK_TOOL_EVENT_PATTERN_UPDATED,
    FEEDBACK_TOOL_EVENT_ERROR_DETECTED,
    FEEDBACK_TOOL_EVENT_HEALTH_CHECK
} feedback_tool_event_id_t;

/**
 * @brief Constitutional event data structure
 */
typedef struct {
    feedback_tool_event_id_t type;
    union {
        struct {
            feedback_state_t old_state;
            feedback_state_t new_state;
            uint32_t timestamp_ms;
        } state_change;
        
        struct {
            feedback_pattern_t pattern;
            uint8_t brightness;
            uint32_t duration_ms;
        } pattern_update;
        
        struct {
            esp_err_t error_code;
            char error_message[64];
            uint32_t timestamp_ms;
        } error_info;
        
        struct {
            bool health_ok;
            uint32_t uptime_ms;
            uint32_t error_count;
        } health_status;
    } data;
} feedback_tool_event_data_t;

// =============================================================================
// Constitutional Tool Interface Functions
// =============================================================================

/**
 * @brief Get constitutional tool identification
 */
const char* feedback_tool_get_id(void);

/**
 * @brief Get constitutional tool version
 */
const char* feedback_tool_get_version(void);

/**
 * @brief Get constitutional tool capabilities
 */
feedback_tool_capabilities_t feedback_tool_get_capabilities(feedback_tool_handle_t handle);

/**
 * @brief Create constitutional default configuration
 */
feedback_tool_config_t feedback_tool_create_default_config(void);

/**
 * @brief Initialize constitutional feedback tool
 * @param config Tool configuration
 * @return Handle on success, NULL on failure
 */
feedback_tool_handle_t feedback_tool_init(const feedback_tool_config_t *config);

/**
 * @brief Deinitialize constitutional feedback tool
 * @param handle Tool handle
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_deinit(feedback_tool_handle_t handle);

/**
 * @brief Get constitutional tool status
 * @param handle Tool handle
 * @param status Output status structure
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_get_status(feedback_tool_handle_t handle, feedback_tool_status_t *status);

// =============================================================================
// Constitutional Visual Feedback Functions
// =============================================================================

/**
 * @brief Set constitutional visual state
 * @param handle Tool handle
 * @param state Visual state to display
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_set_state(feedback_tool_handle_t handle, feedback_state_t state);

/**
 * @brief Set constitutional visual pattern
 * @param handle Tool handle
 * @param pattern Visual pattern type
 * @param brightness LED brightness (0-255)
 * @param duration_ms Pattern duration (0 = permanent)
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_set_pattern(feedback_tool_handle_t handle, 
                                   feedback_pattern_t pattern,
                                   uint8_t brightness,
                                   uint32_t duration_ms);

/**
 * @brief Constitutional health check
 * @param handle Tool handle
 * @return ESP_OK if healthy
 */
esp_err_t feedback_tool_health_check(feedback_tool_handle_t handle);

/**
 * @brief Generate constitutional dashboard
 * @param handle Tool handle
 * @param dashboard_buffer Output buffer (must be 1KB+)
 * @param buffer_size Buffer size
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_generate_dashboard(feedback_tool_handle_t handle, 
                                          char* dashboard_buffer, 
                                          size_t buffer_size);

// =============================================================================
// Constitutional Flow Awareness Functions
// =============================================================================

/**
 * @brief Set constitutional flow awareness context
 * @param handle Tool handle
 * @param flow_active 60-minute flow awareness
 * @param flow_urgent 90-minute flow urgency
 * @return ESP_OK on success
 */
esp_err_t feedback_tool_set_flow_context(feedback_tool_handle_t handle, 
                                        bool flow_active, 
                                        bool flow_urgent);

// =============================================================================
// Constitutional Utility Functions
// =============================================================================

/**
 * @brief Convert state to constitutional string representation
 * @param state Feedback state
 * @return String representation
 */
const char* feedback_tool_state_to_string(feedback_state_t state);

/**
 * @brief Convert pattern to constitutional string representation
 * @param pattern Feedback pattern
 * @return String representation
 */
const char* feedback_tool_pattern_to_string(feedback_pattern_t pattern);

#ifdef __cplusplus
}
#endif