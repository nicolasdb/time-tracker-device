/**
 * @file fs_tool.h
 * @brief Constitutional Filesystem Tool - LittleFS Management with ESP_EVENT Integration
 * 
 * Constitutional implementation of filesystem tool following constitutional patterns:
 * - Handle-based design (no static globals)
 * - ESP_EVENT-only communication
 * - Constitutional memory safety (snprintf)
 * - Container isolation principles
 * 
 * Constitutional Authority: File system operations support for all constitutional tools
 * Architecture Pattern: Handle-based, ESP_EVENT communication, zero coupling
 */

#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// Constitutional FS Tool Event Base
ESP_EVENT_DECLARE_BASE(FS_TOOL_EVENTS);

// Constitutional FS Tool Events
typedef enum {
    FS_TOOL_EVENT_MOUNTED,           // Filesystem mounted successfully
    FS_TOOL_EVENT_UNMOUNTED,         // Filesystem unmounted
    FS_TOOL_EVENT_FORMAT_STARTED,    // Formatting started
    FS_TOOL_EVENT_FORMAT_COMPLETED,  // Formatting completed
    FS_TOOL_EVENT_ERROR,            // Filesystem error occurred
    FS_TOOL_EVENT_CONFIG_SAVED,     // Configuration saved
    FS_TOOL_EVENT_CONFIG_LOADED,    // Configuration loaded
    FS_TOOL_EVENT_LOG_SAVED,        // Log data saved
    FS_TOOL_EVENT_SPACE_WARNING,    // Low disk space warning
    FS_TOOL_EVENT_HEALTH_CHECK,     // Health check completed
} fs_tool_event_t;

// Constitutional FS Tool Capabilities
typedef enum {
    FS_TOOL_CAP_MOUNT             = (1 << 0),  // Filesystem mount/unmount
    FS_TOOL_CAP_AUTO_FORMAT       = (1 << 1),  // Auto-format on mount failure
    FS_TOOL_CAP_JSON_CONFIG       = (1 << 2),  // JSON configuration storage
    FS_TOOL_CAP_JSON_LOG          = (1 << 3),  // JSON log storage
    FS_TOOL_CAP_SPACE_MONITOR     = (1 << 4),  // Disk space monitoring
    FS_TOOL_CAP_EVENT_PUBLISH     = (1 << 5),  // ESP_EVENT publishing
    FS_TOOL_CAP_HEALTH_CHECK      = (1 << 6),  // Filesystem health monitoring
    FS_TOOL_CAP_ATOMIC_OPERATIONS = (1 << 7),  // Atomic file operations
} fs_tool_capabilities_t;

// Constitutional FS Tool Configuration
typedef struct {
    char mount_point[32];              // Mount point (e.g., "/littlefs")
    char partition_label[16];          // Partition label
    bool format_if_mount_failed;       // Auto-format on mount failure
    bool auto_mount_on_init;           // Mount filesystem during init
    bool publish_events;               // Enable ESP_EVENT publishing
    uint32_t space_check_interval_ms;  // Space monitoring interval
    uint8_t low_space_threshold_percent; // Low space warning threshold
    uint32_t file_timeout_ms;          // File operation timeout
    bool use_atomic_operations;        // Enable atomic file operations
    bool create_backup_files;          // Create .bak files for safety
} fs_tool_config_t;

// Constitutional FS Tool Status
typedef struct {
    bool is_initialized;              // Tool initialization status
    bool is_active;                   // Tool active status
    bool is_mounted;                  // Filesystem mount status
    char mount_point[32];             // Current mount point
    char partition_label[16];         // Partition label
    uint32_t total_bytes;             // Total filesystem size
    uint32_t used_bytes;              // Used filesystem space
    uint32_t available_bytes;         // Available filesystem space
    uint8_t usage_percent;            // Usage percentage
    uint32_t uptime_ms;               // Tool uptime
    uint32_t file_operations_count;   // Total file operations
    uint32_t error_count;             // Error count
    fs_tool_capabilities_t capabilities; // Tool capabilities
} fs_tool_status_t;

// Constitutional FS Tool Event Data
typedef struct {
    fs_tool_event_t type;
    union {
        struct {
            char mount_point[32];
            char partition_label[16];
            uint32_t total_bytes;
            uint32_t used_bytes;
        } mount_info;
        struct {
            char file_path[128];
            uint32_t file_size;
            bool success;
        } file_info;
        struct {
            esp_err_t error_code;
            const char* error_message;
            char file_path[128];
        } error_info;
        struct {
            uint32_t total_bytes;
            uint32_t used_bytes;
            uint32_t available_bytes;
            uint8_t usage_percent;
        } space_info;
    } data;
} fs_tool_event_data_t;

// Constitutional FS Tool Handle (Handle-based pattern)
typedef struct fs_tool* fs_tool_handle_t;

/**
 * @brief Get constitutional FS tool identifier
 * @return Constitutional tool ID string
 */
const char* fs_tool_get_id(void);

/**
 * @brief Get constitutional FS tool version
 * @return Constitutional version string
 */
const char* fs_tool_get_version(void);

/**
 * @brief Get constitutional FS tool capabilities
 * @param handle Constitutional tool handle
 * @return Constitutional capabilities bitmask
 */
fs_tool_capabilities_t fs_tool_get_capabilities(fs_tool_handle_t handle);

/**
 * @brief Create default constitutional FS tool configuration
 * @return Constitutional default configuration
 */
fs_tool_config_t fs_tool_create_default_config(void);

/**
 * @brief Initialize constitutional FS tool
 * @param config Constitutional tool configuration
 * @return Constitutional tool handle or NULL on failure
 */
fs_tool_handle_t fs_tool_init(const fs_tool_config_t *config);

/**
 * @brief Deinitialize constitutional FS tool
 * @param handle Constitutional tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_deinit(fs_tool_handle_t handle);

/**
 * @brief Get constitutional FS tool status
 * @param handle Constitutional tool handle
 * @param status Pointer to constitutional status structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_get_status(fs_tool_handle_t handle, fs_tool_status_t *status);

/**
 * @brief Mount constitutional filesystem
 * @param handle Constitutional tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_mount(fs_tool_handle_t handle);

/**
 * @brief Unmount constitutional filesystem
 * @param handle Constitutional tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_unmount(fs_tool_handle_t handle);

/**
 * @brief Format constitutional filesystem
 * @param handle Constitutional tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_format(fs_tool_handle_t handle);

/**
 * @brief Check if constitutional filesystem is mounted
 * @param handle Constitutional tool handle
 * @return true if mounted, false otherwise
 */
bool fs_tool_is_mounted(fs_tool_handle_t handle);

/**
 * @brief Get constitutional filesystem space information
 * @param handle Constitutional tool handle
 * @param total_bytes Pointer to store total bytes
 * @param used_bytes Pointer to store used bytes
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_get_space_info(fs_tool_handle_t handle, uint32_t *total_bytes, uint32_t *used_bytes);

/**
 * @brief Save JSON configuration with constitutional safety
 * @param handle Constitutional tool handle
 * @param filename Configuration filename (constitutional memory safety)
 * @param json_object JSON object to save
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_save_json_config(fs_tool_handle_t handle, const char *filename, const cJSON *json_object);

/**
 * @brief Load JSON configuration with constitutional safety
 * @param handle Constitutional tool handle
 * @param filename Configuration filename (constitutional memory safety)
 * @param json_object Pointer to store loaded JSON object (caller must free)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_load_json_config(fs_tool_handle_t handle, const char *filename, cJSON **json_object);

/**
 * @brief Save JSON log data with constitutional safety
 * @param handle Constitutional tool handle
 * @param filename Log filename (constitutional memory safety)
 * @param json_object JSON object to save
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_save_json_log(fs_tool_handle_t handle, const char *filename, const cJSON *json_object);

/**
 * @brief Load JSON log data with constitutional safety
 * @param handle Constitutional tool handle
 * @param filename Log filename (constitutional memory safety)
 * @param json_object Pointer to store loaded JSON object (caller must free)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_load_json_log(fs_tool_handle_t handle, const char *filename, cJSON **json_object);

/**
 * @brief Append JSON entry to log file with constitutional safety
 * @param handle Constitutional tool handle
 * @param filename Log filename (constitutional memory safety)
 * @param json_entry JSON entry to append
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_append_json_log(fs_tool_handle_t handle, const char *filename, const cJSON *json_entry);

/**
 * @brief Check if file exists with constitutional safety
 * @param handle Constitutional tool handle
 * @param filename File path (constitutional memory safety)
 * @return true if file exists, false otherwise
 */
bool fs_tool_file_exists(fs_tool_handle_t handle, const char *filename);

/**
 * @brief Get file size with constitutional safety
 * @param handle Constitutional tool handle
 * @param filename File path (constitutional memory safety)
 * @param size Pointer to store file size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_get_file_size(fs_tool_handle_t handle, const char *filename, uint32_t *size);

/**
 * @brief Delete file with constitutional safety
 * @param handle Constitutional tool handle
 * @param filename File path (constitutional memory safety)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_delete_file(fs_tool_handle_t handle, const char *filename);

/**
 * @brief Create directory with constitutional safety
 * @param handle Constitutional tool handle
 * @param dirname Directory path (constitutional memory safety)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_create_directory(fs_tool_handle_t handle, const char *dirname);

/**
 * @brief Perform constitutional filesystem health check
 * @param handle Constitutional tool handle
 * @return ESP_OK if healthy, error code otherwise
 */
esp_err_t fs_tool_health_check(fs_tool_handle_t handle);

/**
 * @brief Generate constitutional FS tool dashboard
 * @param handle Constitutional tool handle
 * @param dashboard_buffer Output buffer for dashboard (1KB+ constitutional requirement)
 * @param buffer_size Size of dashboard buffer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_generate_dashboard(fs_tool_handle_t handle, char* dashboard_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif