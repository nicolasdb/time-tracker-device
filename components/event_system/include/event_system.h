/**
 * @file event_system.h
 * @brief Universal Event System for Tool Communication
 * 
 * Defines all event types and data structures for async tool communication.
 * Replaces synchronous function calls with ESP event system.
 * 
 * Process Map Authority: Implements true async event-driven architecture
 * per constitutional requirements and updated process maps.
 */

#pragma once

#include "esp_event.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Include tool headers for type definitions
#include "feedback_tool.h"
#include "payload_tool.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Event Base Declarations (One per tool domain)
// =============================================================================

ESP_EVENT_DECLARE_BASE(SYSTEM_EVENTS);      // System lifecycle events
ESP_EVENT_DECLARE_BASE(RFID_EVENTS);        // RFID tag detection events  
ESP_EVENT_DECLARE_BASE(SESSION_EVENTS);     // Session timing events
ESP_EVENT_DECLARE_BASE(FEEDBACK_EVENTS);    // Visual feedback events
ESP_EVENT_DECLARE_BASE(PAYLOAD_EVENTS);     // Event formatting events
ESP_EVENT_DECLARE_BASE(FS_EVENTS);          // Filesystem operation events
ESP_EVENT_DECLARE_BASE(NETWORK_EVENTS);     // Network connectivity events
ESP_EVENT_DECLARE_BASE(HTTP_EVENTS);        // HTTP transmission events
ESP_EVENT_DECLARE_BASE(NTP_EVENTS);         // Time synchronization events

// =============================================================================
// System Events (Boot sequence, grace period, shutdown)
// =============================================================================

typedef enum {
    SYSTEM_EVENT_BOOT_START = 0,
    SYSTEM_EVENT_GRACE_PERIOD_START,
    SYSTEM_EVENT_GRACE_PERIOD_END,
    SYSTEM_EVENT_READY,
    SYSTEM_EVENT_SHUTDOWN
} system_event_id_t;

typedef struct {
    uint64_t timestamp_us;
    uint32_t grace_period_ms;
} system_event_data_t;

// =============================================================================
// RFID Events (Tag detection, removal, errors)
// =============================================================================

typedef enum {
    RFID_EVENT_READY = 0,
    RFID_EVENT_TAG_DETECTED = 1,
    RFID_EVENT_TAG_REMOVED = 2,    // 🔥 CRITICAL TEST: Use sequential ID=2
    RFID_EVENT_TAG_IGNORED = 3,    // Move to ID=3
    RFID_EVENT_ERROR = 4
} rfid_event_id_t;

typedef struct {
    char tag_uid[32];
    uint64_t detection_time_us;
    uint64_t internal_millis;
    bool is_new_session;
    bool during_grace_period;
} rfid_event_data_t;

// =============================================================================
// Session Events (Session timing and flow awareness)
// =============================================================================

typedef enum {
    SESSION_EVENT_STARTED = 0,
    SESSION_EVENT_ENDED,
    SESSION_EVENT_FLOW_AWARENESS,    // 60 minute threshold
    SESSION_EVENT_FLOW_URGENCY,      // 90 minute threshold
    SESSION_EVENT_TIMEOUT
} session_event_id_t;

typedef struct {
    char tag_uid[32];
    uint64_t session_start_time_ms;
    uint64_t session_duration_ms;
    uint32_t session_duration_min;
    bool flow_awareness_active;
    bool flow_urgency_active;
} session_event_data_t;

// =============================================================================
// Feedback Events (Visual state changes)
// =============================================================================

typedef enum {
    FEEDBACK_EVENT_READY = 0,
    FEEDBACK_EVENT_STATE_CHANGE,
    FEEDBACK_EVENT_FLASH_REQUEST,
    FEEDBACK_EVENT_FLOW_CONTEXT_CHANGE,
    FEEDBACK_EVENT_PRIORITY_OVERRIDE
} feedback_event_id_t;

// Type definitions from feedback_tool.h are now available

typedef struct {
    feedback_state_t state;
    feedback_priority_t priority;
    uint32_t duration_ms;        // 0 = permanent
    uint32_t flash_count;        // For flash requests
    bool flow_awareness_active;  // Flow context
    bool flow_urgency_active;    // Flow context
} feedback_event_data_t;

// =============================================================================
// Payload Events (Event formatting and preparation)
// =============================================================================

typedef enum {
    PAYLOAD_EVENT_READY = 0,
    PAYLOAD_EVENT_FORMAT_REQUEST,
    PAYLOAD_EVENT_FORMATTED,
    PAYLOAD_EVENT_ERROR
} payload_event_id_t;

// Type definitions from payload_tool.h are now available

// =============================================================================
// Filesystem Events (Storage operations)
// =============================================================================

typedef enum {
    FS_EVENT_READY = 0,
    FS_EVENT_SAVE_REQUEST,
    FS_EVENT_SAVED,
    FS_EVENT_DELETE_REQUEST,
    FS_EVENT_DELETED,
    FS_EVENT_ERROR,
    FS_EVENT_SPACE_WARNING
} fs_event_id_t;

typedef struct {
    char filename[64];
    char json_data[512];
    uint32_t file_size;
    bool operation_success;
    uint32_t used_space_percent;
} fs_event_data_t;

// =============================================================================
// Network Events (WiFi connectivity)
// =============================================================================

typedef enum {
    NETWORK_EVENT_READY = 0,
    NETWORK_EVENT_CONNECTING,
    NETWORK_EVENT_CONNECTED,
    NETWORK_EVENT_DISCONNECTED,
    NETWORK_EVENT_FAILED,
    NETWORK_EVENT_OFFLINE
} network_event_id_t;

typedef struct {
    char ssid[32];
    int8_t rssi;
    bool is_connected;
    uint32_t ip_address;
    uint32_t retry_count;
} network_event_data_t;

// =============================================================================
// HTTP Events (Data transmission)
// =============================================================================

typedef enum {
    HTTP_EVENT_READY = 0,
    HTTP_EVENT_SEND_REQUEST,
    HTTP_EVENT_SUCCESS,
    HTTP_EVENT_FAILED,
    HTTP_EVENT_RETRY_SCHEDULED
} http_event_id_t;

typedef struct {
    char url[128];
    char payload_json[512];
    uint16_t response_code;
    uint32_t retry_count;
    uint32_t retry_delay_ms;
    bool transmission_success;
} http_event_data_t;

// =============================================================================
// NTP Events (Time synchronization)
// =============================================================================

typedef enum {
    NTP_EVENT_READY = 0,
    NTP_EVENT_SYNC_REQUEST,
    NTP_EVENT_SYNCED,
    NTP_EVENT_SYNC_FAILED
} ntp_event_id_t;

typedef struct {
    uint64_t ntp_timestamp_ms;
    int32_t timezone_offset_sec;
    bool sync_success;
    uint32_t sync_accuracy_ms;
} ntp_event_data_t;

// =============================================================================
// Event System Initialization
// =============================================================================

/**
 * @brief Initialize the universal event system
 * @return ESP_OK on success
 */
esp_err_t event_system_init(void);

/**
 * @brief Cleanup event system resources
 * @return ESP_OK on success
 */
esp_err_t event_system_deinit(void);

/**
 * @brief Register all event bases with the ESP event system
 * @return ESP_OK on success
 */
esp_err_t event_system_register_all_bases(void);

// =============================================================================
// Event Publishing Helpers
// =============================================================================

/**
 * @brief Publish system event
 */
esp_err_t publish_system_event(system_event_id_t event_id, system_event_data_t* data);

/**
 * @brief Publish RFID event
 */
esp_err_t publish_rfid_event(rfid_event_id_t event_id, rfid_event_data_t* data);

/**
 * @brief Publish session event
 */
esp_err_t publish_session_event(session_event_id_t event_id, session_event_data_t* data);

/**
 * @brief Publish feedback event
 */
esp_err_t publish_feedback_event(feedback_event_id_t event_id, feedback_event_data_t* data);

/**
 * @brief Publish payload event
 */
esp_err_t publish_payload_event(payload_event_id_t event_id, payload_event_data_t* data);

/**
 * @brief Publish filesystem event
 */
esp_err_t publish_fs_event(fs_event_id_t event_id, fs_event_data_t* data);

/**
 * @brief Publish network event
 */
esp_err_t publish_network_event(network_event_id_t event_id, network_event_data_t* data);

/**
 * @brief Publish HTTP event
 */
esp_err_t publish_http_event(http_event_id_t event_id, http_event_data_t* data);

/**
 * @brief Publish NTP event
 */
esp_err_t publish_ntp_event(ntp_event_id_t event_id, ntp_event_data_t* data);

// =============================================================================
// Event Subscription Helpers
// =============================================================================

/**
 * @brief Subscribe to system events
 */
esp_err_t subscribe_to_system_events(esp_event_handler_t handler, void* handler_arg);

/**
 * @brief Subscribe to RFID events
 */
esp_err_t subscribe_to_rfid_events(esp_event_handler_t handler, void* handler_arg);

/**
 * @brief Subscribe to session events  
 */
esp_err_t subscribe_to_session_events(esp_event_handler_t handler, void* handler_arg);

/**
 * @brief Subscribe to feedback events
 */
esp_err_t subscribe_to_feedback_events(esp_event_handler_t handler, void* handler_arg);

/**
 * @brief Subscribe to payload events
 */
esp_err_t subscribe_to_payload_events(esp_event_handler_t handler, void* handler_arg);

/**
 * @brief Subscribe to filesystem events
 */
esp_err_t subscribe_to_fs_events(esp_event_handler_t handler, void* handler_arg);

/**
 * @brief Subscribe to network events
 */
esp_err_t subscribe_to_network_events(esp_event_handler_t handler, void* handler_arg);

/**
 * @brief Subscribe to HTTP events
 */
esp_err_t subscribe_to_http_events(esp_event_handler_t handler, void* handler_arg);

/**
 * @brief Subscribe to NTP events
 */
esp_err_t subscribe_to_ntp_events(esp_event_handler_t handler, void* handler_arg);

#ifdef __cplusplus
}
#endif

/**
 * @brief Usage Example - True Async Event-Driven Architecture
 * 
 * // In RFID task - publish tag detection
 * rfid_event_data_t rfid_data = {
 *     .tag_uid = "A1B2C3D4",
 *     .detection_time_us = esp_timer_get_time(),
 *     .is_new_session = true
 * };
 * publish_rfid_event(RFID_EVENT_TAG_DETECTED, &rfid_data);
 * 
 * // In system monitor task - subscribe to RFID events
 * esp_err_t system_monitor_event_handler(void* handler_args, esp_event_base_t base, 
 *                                        int32_t id, void* event_data) {
 *     if (base == RFID_EVENTS && id == RFID_EVENT_TAG_DETECTED) {
 *         rfid_event_data_t* rfid_data = (rfid_event_data_t*)event_data;
 *         // Start session timing
 *         session_event_data_t session_data = {
 *             .session_start_time_ms = esp_timer_get_time() / 1000,
 *             .tag_uid = rfid_data->tag_uid
 *         };
 *         publish_session_event(SESSION_EVENT_STARTED, &session_data);
 *     }
 *     return ESP_OK;
 * }
 * 
 * // In feedback task - subscribe to session events  
 * esp_err_t feedback_event_handler(void* handler_args, esp_event_base_t base,
 *                                  int32_t id, void* event_data) {
 *     if (base == SESSION_EVENTS && id == SESSION_EVENT_STARTED) {
 *         feedback_event_data_t feedback_data = {
 *             .state = FEEDBACK_STATE_TAG_DETECTED,
 *             .priority = FEEDBACK_PRIORITY_HIGH,
 *             .flow_awareness_active = false  // Fresh session
 *         };
 *         publish_feedback_event(FEEDBACK_EVENT_STATE_CHANGE, &feedback_data);
 *     }
 *     return ESP_OK;
 * }
 */