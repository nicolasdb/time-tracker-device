/**
 * @file ntp_tool.h
 * @brief Constitutional NTP Tool Interface - Time Synchronization
 * 
 * Constitutional implementation following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf, PRIu32)
 * - Container isolation principles
 * 
 * Constitutional Authority: Process Map 13 dependency - "wait for NTP_SYNC" → "clock synced"
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"
#include "esp_event.h"
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constitutional NTP Tool
// =============================================================================

/**
 * @brief Constitutional NTP tool handle
 */
typedef struct ntp_tool* ntp_tool_handle_t;

/**
 * @brief NTP synchronization state
 */
typedef enum {
    NTP_STATE_NOT_SYNCED,
    NTP_STATE_SYNCING,
    NTP_STATE_SYNCED,
    NTP_STATE_FAILED
} ntp_state_t;

/**
 * @brief NTP server configuration
 */
typedef struct {
    char hostname[64];
    uint8_t priority;  // 0-255, higher = more preferred
} ntp_server_config_t;

/**
 * @brief NTP tool configuration
 */
typedef struct {
    ntp_server_config_t servers[4];  // Up to 4 NTP servers
    uint32_t sync_interval_s;        // Sync interval in seconds
    uint32_t sync_timeout_ms;        // Sync timeout in milliseconds
    uint32_t retry_attempts;         // Max retry attempts
    char timezone[64];               // POSIX timezone string
    bool auto_sync_enabled;          // Enable automatic periodic sync
    bool wifi_triggered_sync;        // Sync when WiFi connects
    bool publish_events;             // Publish ESP_EVENT messages
} ntp_tool_config_t;

/**
 * @brief NTP tool status
 */
typedef struct {
    bool is_initialized;
    bool is_active;
    ntp_state_t sync_state;
    bool time_valid;
    time_t last_sync_time;
    time_t system_time;
    int64_t time_offset_us;
    uint32_t sync_count;
    uint32_t sync_failures;
    char current_server[64];
    uint64_t last_sync_timestamp_us;
} ntp_tool_status_t;

// =============================================================================
// Constitutional NTP Events
// =============================================================================

ESP_EVENT_DECLARE_BASE(NTP_TOOL_EVENTS);

typedef enum {
    NTP_TOOL_EVENT_SYNC_STARTED = 0,
    NTP_TOOL_EVENT_SYNC_SUCCESS,
    NTP_TOOL_EVENT_SYNC_FAILED,
    NTP_TOOL_EVENT_TIME_UPDATED,
    NTP_TOOL_EVENT_TIMEZONE_CHANGED
} ntp_tool_event_id_t;

typedef struct {
    ntp_state_t state;
    time_t system_time;
    int64_t time_offset_us;
    char server_used[64];
    uint32_t response_time_ms;
    uint64_t timestamp_us;
} ntp_tool_event_t;

// =============================================================================
// Constitutional NTP Interface
// =============================================================================

/**
 * @brief Get constitutional NTP tool identification
 */
const char* ntp_tool_get_id(void);

/**
 * @brief Get constitutional NTP tool version
 */
const char* ntp_tool_get_version(void);

/**
 * @brief Create default NTP configuration
 */
ntp_tool_config_t ntp_tool_create_default_config(void);

/**
 * @brief Initialize constitutional NTP tool
 */
ntp_tool_handle_t ntp_tool_init(const ntp_tool_config_t *config);

/**
 * @brief Deinitialize constitutional NTP tool
 */
esp_err_t ntp_tool_deinit(ntp_tool_handle_t handle);

/**
 * @brief Get constitutional NTP tool status
 */
esp_err_t ntp_tool_get_status(ntp_tool_handle_t handle, ntp_tool_status_t *status);

/**
 * @brief Manually trigger NTP synchronization
 */
esp_err_t ntp_tool_sync_now(ntp_tool_handle_t handle);

/**
 * @brief Check if time is synchronized and valid
 */
bool ntp_tool_is_time_valid(ntp_tool_handle_t handle);

/**
 * @brief Get current system time (only valid after sync)
 */
esp_err_t ntp_tool_get_time(ntp_tool_handle_t handle, time_t *current_time);

/**
 * @brief Get precise timestamp in microseconds (for payload creation)
 */
esp_err_t ntp_tool_get_precise_timestamp(ntp_tool_handle_t handle, uint64_t *timestamp_us);

/**
 * @brief Get NTP sync correlation data for timestamp calculation
 * Implements Issue #6 requirement for real timestamp calculation
 * 
 * @param handle NTP tool handle
 * @param real_time_at_sync Output real time when sync occurred (Unix time_t)
 * @param esp_timer_at_sync Output esp_timer value when sync occurred
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_STATE if no sync
 */
esp_err_t ntp_tool_get_sync_correlation(ntp_tool_handle_t handle, 
                                       time_t *real_time_at_sync, 
                                       uint64_t *esp_timer_at_sync);

/**
 * @brief Set timezone configuration
 */
esp_err_t ntp_tool_set_timezone(ntp_tool_handle_t handle, const char* timezone);

#ifdef __cplusplus
}
#endif

/**
 * @brief Constitutional NTP Tool Example
 * 
 * // Initialize NTP tool
 * ntp_tool_config_t config = ntp_tool_create_default_config();
 * ntp_tool_handle_t ntp = ntp_tool_init(&config);
 * 
 * // Wait for automatic WiFi-triggered sync
 * // Or manually trigger sync
 * ntp_tool_sync_now(ntp);
 * 
 * // Check time validity (required for payload_tool per Process Map 13)
 * if (ntp_tool_is_time_valid(ntp)) {
 *     uint64_t precise_timestamp;
 *     ntp_tool_get_precise_timestamp(ntp, &precise_timestamp);
 *     // Use timestamp for payload creation
 * }
 * 
 * // Cleanup
 * ntp_tool_deinit(ntp);
 */