/**
 * @file tool_registry.h
 * @brief Constitutional Tool Registry System
 * 
 * ESP_EVENT-based tool management for container architecture.
 * Constitutional Authority: Process Map 01
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constitutional Configuration
// =============================================================================

#define TOOL_REGISTRY_MAX_TOOLS     16
#define TOOL_REGISTRY_MAX_ID_LENGTH 32
#define TOOL_REGISTRY_MAX_VERSION_LENGTH 16

/**
 * @brief Tool lifecycle states per Process Map 01
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
 * @brief Tool capabilities bitmask
 */
typedef uint32_t tool_capabilities_t;

/**
 * @brief Constitutional tool interface (ESP_EVENT only)
 */
typedef struct {
    const char* (*get_id)(void);
    const char* (*get_version)(void);
    tool_capabilities_t (*get_capabilities)(void* handle);
    esp_err_t (*get_status)(void* handle, void* status);
    esp_err_t (*cleanup)(void* handle);
} tool_interface_t;

/**
 * @brief Tool registration entry
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
    bool is_critical;
} tool_registration_entry_t;

/**
 * @brief Registry statistics
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
// Constitutional Tool Registry Interface
// =============================================================================

/**
 * @brief Initialize tool registry
 * @return ESP_OK on success
 */
esp_err_t tool_registry_init(void);

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
 * @brief Get tool handle by ID
 * @param tool_id Tool identifier
 * @param tool_handle Output tool handle
 * @return ESP_OK on success
 */
esp_err_t tool_registry_get_handle(const char* tool_id, void** tool_handle);

/**
 * @brief Set tool state by ID
 * @param tool_id Tool identifier
 * @param state New tool state
 * @return ESP_OK on success
 */
esp_err_t tool_registry_set_state(const char* tool_id, tool_state_t state);

/**
 * @brief Get registry statistics
 * @param stats Output statistics structure
 * @return ESP_OK on success
 */
esp_err_t tool_registry_get_stats(tool_registry_stats_t* stats);

/**
 * @brief Cleanup tool registry
 * @return ESP_OK on success
 */
esp_err_t tool_registry_cleanup(void);

// =============================================================================
// Process Map Boot Sequence Support
// =============================================================================

/**
 * @brief Tool initialization order per Process Map 01
 */
typedef enum {
    TOOL_INIT_ORDER_FS_TOOL         = 1,
    TOOL_INIT_ORDER_SYSTEM_MONITOR  = 2,
    TOOL_INIT_ORDER_FEEDBACK        = 3,
    TOOL_INIT_ORDER_NETWORK         = 4,
    TOOL_INIT_ORDER_NTP             = 5,
    TOOL_INIT_ORDER_PAYLOAD         = 6,
    TOOL_INIT_ORDER_RFID            = 7,
    TOOL_INIT_ORDER_HTTP            = 8,
    TOOL_INIT_ORDER_WEBSERVER       = 9
} tool_init_order_t;

/**
 * @brief Boot sequence validation entry
 */
typedef struct {
    const char* tool_id;
    tool_init_order_t order;
    bool is_initialized;
    esp_err_t init_result;
} boot_sequence_entry_t;

/**
 * @brief Validate boot sequence per Process Map 01
 * @param sequence Boot sequence entries
 * @param sequence_length Number of entries
 * @return ESP_OK if constitutional compliance achieved
 */
esp_err_t tool_registry_boot_sequence_init(boot_sequence_entry_t* sequence,
                                          uint32_t sequence_length);

#ifdef __cplusplus
}
#endif