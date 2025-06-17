/**
 * @file fs_tool.h
 * @brief MCP-Inspired Filesystem Tool Interface
 * 
 * Self-contained LittleFS management tool providing JSON config/log APIs.
 * Other tools depend on fs_tool for persistent storage instead of direct filesystem calls.
 * Follows MCP patterns with handle-based lifecycle and managed joltwallet/littlefs component.
 */

#ifndef FS_TOOL_H
#define FS_TOOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// MCP Tool Metadata & Constants
// =============================================================================

#define FS_TOOL_ID               "fs"
#define FS_TOOL_VERSION          "1.0.0"
#define FS_TOOL_DESCRIPTION      "MCP-inspired LittleFS tool with JSON config/log APIs"

#define FS_TOOL_MAX_PATH_LEN     128
#define FS_TOOL_MAX_MOUNT_LEN    32
#define FS_TOOL_MAX_LABEL_LEN    16
#define FS_TOOL_DEFAULT_TIMEOUT  5000

// =============================================================================
// MCP Tool Events System
// =============================================================================

ESP_EVENT_DECLARE_BASE(FS_TOOL_EVENTS);

/**
 * @brief Filesystem Tool Event Types (Published for other tools)
 */
typedef enum {
    FS_TOOL_EVENT_MOUNTED = 0,              ///< Filesystem mounted successfully
    FS_TOOL_EVENT_UNMOUNTED,                ///< Filesystem unmounted
    FS_TOOL_EVENT_FORMAT_STARTED,           ///< Formatting started
    FS_TOOL_EVENT_FORMAT_COMPLETED,         ///< Formatting completed
    FS_TOOL_EVENT_ERROR,                    ///< Filesystem error occurred
    FS_TOOL_EVENT_CONFIG_SAVED,             ///< Configuration saved to file
    FS_TOOL_EVENT_CONFIG_LOADED,            ///< Configuration loaded from file
    FS_TOOL_EVENT_LOG_SAVED,                ///< Log data saved to file
    FS_TOOL_EVENT_LOG_LOADED,               ///< Log data loaded from file
    FS_TOOL_EVENT_SPACE_WARNING,            ///< Low disk space warning
} fs_tool_event_type_t;

/**
 * @brief Filesystem Tool Event Data Structure
 */
typedef struct {
    fs_tool_event_type_t type;
    union {
        struct {
            char mount_point[FS_TOOL_MAX_MOUNT_LEN];
            char partition_label[FS_TOOL_MAX_LABEL_LEN];
            uint32_t total_bytes;
            uint32_t used_bytes;
        } mount_info;
        struct {
            char file_path[FS_TOOL_MAX_PATH_LEN];
            uint32_t file_size;
            bool success;
        } file_info;
        struct {
            esp_err_t error_code;
            const char* error_message;
            char file_path[FS_TOOL_MAX_PATH_LEN];
        } error_info;
        struct {
            uint32_t total_bytes;
            uint32_t used_bytes;
            uint32_t available_bytes;
            uint8_t usage_percent;
        } space_info;
    } data;
} fs_tool_event_t;

// =============================================================================
// MCP Tool Capabilities & Configuration
// =============================================================================

/**
 * @brief Filesystem Tool Capabilities (Bitmask)
 */
typedef enum {
    FS_CAP_MOUNT             = (1 << 0),    ///< Filesystem mount/unmount
    FS_CAP_AUTO_FORMAT       = (1 << 1),    ///< Auto-format on mount failure
    FS_CAP_JSON_CONFIG       = (1 << 2),    ///< JSON configuration storage
    FS_CAP_JSON_LOG          = (1 << 3),    ///< JSON log storage
    FS_CAP_SPACE_MONITOR     = (1 << 4),    ///< Disk space monitoring
    FS_CAP_EVENT_PUBLISH     = (1 << 5),    ///< Event publishing
    FS_CAP_HEALTH_CHECK      = (1 << 6),    ///< Filesystem health monitoring
    FS_CAP_ATOMIC_OPERATIONS = (1 << 7),    ///< Atomic file operations
} fs_tool_capabilities_t;

/**
 * @brief Filesystem Tool Configuration
 */
typedef struct {
    // Mount Configuration
    char mount_point[FS_TOOL_MAX_MOUNT_LEN]; ///< Mount point (e.g., "/littlefs")
    char partition_label[FS_TOOL_MAX_LABEL_LEN]; ///< Partition label (NULL = default)
    bool format_if_mount_failed;             ///< Auto-format on mount failure
    
    // Behavior Settings
    bool auto_mount_on_init;                 ///< Mount filesystem during init
    bool publish_events;                     ///< Enable event publishing
    uint32_t space_check_interval_ms;        ///< Space monitoring interval
    uint8_t low_space_threshold_percent;     ///< Low space warning threshold
    
    // File Operation Settings
    uint32_t file_timeout_ms;                ///< File operation timeout
    bool use_atomic_operations;              ///< Enable atomic file operations
    bool create_backup_files;                ///< Create .bak files for safety
    
    // Event Publishing
    uint32_t event_task_stack_size;          ///< Event task stack size
} fs_tool_config_t;

// =============================================================================
// MCP Tool Types & Handles
// =============================================================================

/**
 * @brief Opaque Filesystem Tool Handle
 */
typedef struct fs_tool_context* fs_tool_handle_t;

/**
 * @brief Filesystem Tool Status Information
 */
typedef struct {
    bool is_initialized;                     ///< Tool initialization status
    bool is_active;                          ///< Tool active status
    bool is_mounted;                         ///< Filesystem mount status
    char mount_point[FS_TOOL_MAX_MOUNT_LEN]; ///< Current mount point
    char partition_label[FS_TOOL_MAX_LABEL_LEN]; ///< Partition label
    uint32_t total_bytes;                    ///< Total filesystem size
    uint32_t used_bytes;                     ///< Used filesystem space
    uint32_t available_bytes;                ///< Available filesystem space
    uint8_t usage_percent;                   ///< Usage percentage
    uint32_t uptime_ms;                      ///< Tool uptime
    uint32_t file_operations_count;          ///< Total file operations
    uint32_t error_count;                    ///< Error count
    fs_tool_capabilities_t capabilities;     ///< Tool capabilities
} fs_tool_status_t;

/**
 * @brief Filesystem Tool Registry Entry (MCP Pattern)
 */
typedef struct {
    const char* tool_id;
    const char* version;
    const char* description;
    fs_tool_capabilities_t capabilities;
    fs_tool_handle_t (*init_func)(const fs_tool_config_t* config);
    esp_err_t (*deinit_func)(fs_tool_handle_t handle);
} fs_tool_registry_t;

// =============================================================================
// MCP Tool Interface Functions
// =============================================================================

/**
 * @brief Get filesystem tool identifier
 * @return Tool ID string
 */
const char* fs_tool_get_id(void);

/**
 * @brief Get filesystem tool version
 * @return Version string
 */
const char* fs_tool_get_version(void);

/**
 * @brief Create default filesystem tool configuration
 * @return Default configuration structure
 */
fs_tool_config_t fs_tool_create_default_config(void);

/**
 * @brief Initialize filesystem tool with configuration
 * @param config Tool configuration
 * @return Tool handle or NULL on failure
 */
fs_tool_handle_t fs_tool_init(const fs_tool_config_t *config);

/**
 * @brief Deinitialize filesystem tool and free resources
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_deinit(fs_tool_handle_t handle);

/**
 * @brief Get filesystem tool capabilities
 * @param handle Tool handle
 * @return Capabilities bitmask
 */
fs_tool_capabilities_t fs_tool_get_capabilities(fs_tool_handle_t handle);

/**
 * @brief Get filesystem tool status
 * @param handle Tool handle
 * @param status Pointer to status structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_get_status(fs_tool_handle_t handle, fs_tool_status_t *status);

/**
 * @brief Get tool registry entry (MCP pattern)
 * @return Pointer to registry entry
 */
const fs_tool_registry_t* fs_tool_get_registry_entry(void);

// =============================================================================
// Filesystem Operations Interface
// =============================================================================

/**
 * @brief Mount filesystem
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_mount(fs_tool_handle_t handle);

/**
 * @brief Unmount filesystem
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_unmount(fs_tool_handle_t handle);

/**
 * @brief Format filesystem
 * @param handle Tool handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_format(fs_tool_handle_t handle);

/**
 * @brief Check if filesystem is mounted
 * @param handle Tool handle
 * @return true if mounted, false otherwise
 */
bool fs_tool_is_mounted(fs_tool_handle_t handle);

/**
 * @brief Get filesystem space information
 * @param handle Tool handle
 * @param total_bytes Pointer to store total bytes
 * @param used_bytes Pointer to store used bytes
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_get_space_info(fs_tool_handle_t handle, uint32_t *total_bytes, uint32_t *used_bytes);

// =============================================================================
// JSON Configuration API (For Other Tools)
// =============================================================================

/**
 * @brief Save JSON configuration to file
 * @param handle Tool handle
 * @param filename Configuration filename (relative to mount point)
 * @param json_object JSON object to save
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_save_json_config(fs_tool_handle_t handle, const char *filename, const cJSON *json_object);

/**
 * @brief Load JSON configuration from file
 * @param handle Tool handle
 * @param filename Configuration filename (relative to mount point)
 * @param json_object Pointer to store loaded JSON object (caller must free)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_load_json_config(fs_tool_handle_t handle, const char *filename, cJSON **json_object);

/**
 * @brief Save JSON log data to file
 * @param handle Tool handle
 * @param filename Log filename (relative to mount point)
 * @param json_object JSON object to save
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_save_json_log(fs_tool_handle_t handle, const char *filename, const cJSON *json_object);

/**
 * @brief Load JSON log data from file
 * @param handle Tool handle
 * @param filename Log filename (relative to mount point)
 * @param json_object Pointer to store loaded JSON object (caller must free)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_load_json_log(fs_tool_handle_t handle, const char *filename, cJSON **json_object);

/**
 * @brief Append JSON entry to log file
 * @param handle Tool handle
 * @param filename Log filename (relative to mount point)
 * @param json_entry JSON entry to append
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_append_json_log(fs_tool_handle_t handle, const char *filename, const cJSON *json_entry);

// =============================================================================
// File Operations Interface
// =============================================================================

/**
 * @brief Check if file exists
 * @param handle Tool handle
 * @param filename File path (relative to mount point)
 * @return true if file exists, false otherwise
 */
bool fs_tool_file_exists(fs_tool_handle_t handle, const char *filename);

/**
 * @brief Get file size
 * @param handle Tool handle
 * @param filename File path (relative to mount point)
 * @param size Pointer to store file size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_get_file_size(fs_tool_handle_t handle, const char *filename, uint32_t *size);

/**
 * @brief Delete file
 * @param handle Tool handle
 * @param filename File path (relative to mount point)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_delete_file(fs_tool_handle_t handle, const char *filename);

/**
 * @brief Create directory
 * @param handle Tool handle
 * @param dirname Directory path (relative to mount point)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_create_directory(fs_tool_handle_t handle, const char *dirname);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert filesystem tool event type to string
 * @param event_type Event type
 * @return String representation
 */
const char* fs_tool_event_to_string(fs_tool_event_type_t event_type);

/**
 * @brief Get full file path (mount_point + filename)
 * @param handle Tool handle
 * @param filename Relative filename
 * @param full_path Buffer to store full path
 * @param max_len Maximum buffer length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_tool_get_full_path(fs_tool_handle_t handle, const char *filename, char *full_path, size_t max_len);

/**
 * @brief Perform filesystem health check
 * @param handle Tool handle
 * @return ESP_OK if healthy, error code otherwise
 */
esp_err_t fs_tool_health_check(fs_tool_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // FS_TOOL_H