/**
 * @file fs_tool.c
 * @brief MCP-Inspired Filesystem Tool Implementation
 * 
 * Self-contained LittleFS management with managed joltwallet/littlefs component.
 * Provides JSON config/log APIs for other tools, eliminating direct filesystem coupling.
 * Follows MCP patterns with handle-based lifecycle and event publishing.
 */

#include "fs_tool.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_littlefs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static const char *TAG = "FS_TOOL";

// =============================================================================
// Tool Context Structure (Handle-based Design)
// =============================================================================

/**
 * @brief Filesystem Tool Context (Replaces static globals)
 */
struct fs_tool_context {
    // MCP Tool Metadata
    fs_tool_config_t config;
    fs_tool_capabilities_t capabilities;
    bool is_initialized;
    bool is_active;
    uint32_t uptime_start;
    
    // Filesystem State
    bool is_mounted;
    uint32_t total_bytes;
    uint32_t used_bytes;
    uint32_t available_bytes;
    uint8_t usage_percent;
    
    // Operation Tracking
    uint32_t file_operations_count;
    uint32_t error_count;
    uint32_t last_space_check_ms;
    
    // Synchronization
    SemaphoreHandle_t mutex;
    
    // Background monitoring task
    TaskHandle_t monitor_task;
    
    // Event publishing
    esp_event_handler_instance_t event_handler;
};

// =============================================================================
// Forward Declarations
// =============================================================================

static esp_err_t fs_tool_update_space_info(fs_tool_handle_t handle);
static esp_err_t fs_tool_create_full_path(fs_tool_handle_t handle, const char *filename, char *full_path, size_t max_len);
static esp_err_t fs_tool_atomic_file_operation(fs_tool_handle_t handle, const char *filename, const char *data, size_t data_len, bool is_append);
static void fs_tool_monitor_task(void *pvParameters);
static esp_err_t fs_tool_publish_event(fs_tool_handle_t handle, fs_tool_event_type_t event_type, void *event_data, size_t data_size);

// =============================================================================
// MCP Tool Interface Implementation
// =============================================================================

const char* fs_tool_get_id(void) {
    return FS_TOOL_ID;
}

const char* fs_tool_get_version(void) {
    return FS_TOOL_VERSION;
}

fs_tool_config_t fs_tool_create_default_config(void) {
    fs_tool_config_t config = {
        .mount_point = "/littlefs",
        .partition_label = "",  // Empty = use default partition
        .format_if_mount_failed = true,
        .auto_mount_on_init = true,
        .publish_events = true,
        .space_check_interval_ms = 30000,  // Check every 30 seconds
        .low_space_threshold_percent = 85,
        .file_timeout_ms = FS_TOOL_DEFAULT_TIMEOUT,
        .use_atomic_operations = true,
        .create_backup_files = false,
        .event_task_stack_size = 3072,
    };
    return config;
}

fs_tool_handle_t fs_tool_init(const fs_tool_config_t *config) {
    ESP_LOGI(TAG, "Initializing filesystem tool with MCP architecture");
    
    if (config == NULL) {
        ESP_LOGE(TAG, "Configuration is NULL");
        return NULL;
    }
    
    // Allocate context (handle-based design)
    fs_tool_handle_t handle = calloc(1, sizeof(struct fs_tool_context));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate filesystem tool context");
        return NULL;
    }
    
    // Copy configuration
    memcpy(&handle->config, config, sizeof(fs_tool_config_t));
    
    // Initialize MCP metadata
    handle->capabilities = FS_CAP_MOUNT | FS_CAP_AUTO_FORMAT | FS_CAP_JSON_CONFIG |
                          FS_CAP_JSON_LOG | FS_CAP_SPACE_MONITOR | FS_CAP_EVENT_PUBLISH |
                          FS_CAP_HEALTH_CHECK | FS_CAP_ATOMIC_OPERATIONS;
    
    handle->is_initialized = false;
    handle->is_active = false;
    handle->uptime_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Initialize state
    handle->is_mounted = false;
    handle->total_bytes = 0;
    handle->used_bytes = 0;
    handle->available_bytes = 0;
    handle->usage_percent = 0;
    handle->file_operations_count = 0;
    handle->error_count = 0;
    handle->last_space_check_ms = 0;
    
    // Create synchronization primitives
    handle->mutex = xSemaphoreCreateMutex();
    if (handle->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(handle);
        return NULL;
    }
    
    // Auto-mount if configured
    if (handle->config.auto_mount_on_init) {
        esp_err_t err = fs_tool_mount(handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount filesystem during init: %s", esp_err_to_name(err));
            fs_tool_deinit(handle);
            return NULL;
        }
    }
    
    // Create background monitoring task
    if (handle->config.space_check_interval_ms > 0) {
        BaseType_t task_result = xTaskCreate(
            fs_tool_monitor_task,
            "fs_monitor",
            handle->config.event_task_stack_size,
            handle,
            tskIDLE_PRIORITY + 1,
            &handle->monitor_task
        );
        
        if (task_result != pdPASS) {
            ESP_LOGW(TAG, "Failed to create monitor task");
            handle->monitor_task = NULL;
        }
    }
    
    handle->is_initialized = true;
    handle->is_active = true;
    
    ESP_LOGI(TAG, "🗂️ Filesystem tool initialized successfully");
    ESP_LOGI(TAG, "📁 Mount point: %s", handle->config.mount_point);
    ESP_LOGI(TAG, "💾 Partition: %s", strlen(handle->config.partition_label) > 0 ? handle->config.partition_label : "default");
    ESP_LOGI(TAG, "⚡ Auto-format: %s", handle->config.format_if_mount_failed ? "enabled" : "disabled");
    
    return handle;
}

esp_err_t fs_tool_deinit(fs_tool_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Deinitializing filesystem tool");
    
    // Signal task to stop
    handle->is_active = false;
    
    // Wait for monitoring task to exit gracefully
    if (handle->monitor_task) {
        ESP_LOGI(TAG, "Waiting for monitor task to exit...");
        // Give task time to see is_active = false and exit
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Check if task still exists
        eTaskState task_state = eTaskGetState(handle->monitor_task);
        if (task_state != eDeleted) {
            ESP_LOGW(TAG, "Monitor task still running, force deleting");
            vTaskDelete(handle->monitor_task);
        }
        handle->monitor_task = NULL;
    }
    
    // Unmount filesystem safely
    if (handle->is_mounted) {
        esp_err_t unmount_result = fs_tool_unmount(handle);
        if (unmount_result != ESP_OK) {
            ESP_LOGW(TAG, "Unmount failed during deinit: %s", esp_err_to_name(unmount_result));
            // Continue with cleanup even if unmount fails
        }
    }
    
    // Clean up synchronization primitives
    if (handle->mutex) {
        vSemaphoreDelete(handle->mutex);
        handle->mutex = NULL;
    }
    
    // Clear handle contents before freeing
    memset(handle, 0, sizeof(struct fs_tool_context));
    free(handle);
    
    ESP_LOGI(TAG, "Filesystem tool deinitialized");
    return ESP_OK;
}

fs_tool_capabilities_t fs_tool_get_capabilities(fs_tool_handle_t handle) {
    if (handle == NULL) {
        return 0;
    }
    return handle->capabilities;
}

esp_err_t fs_tool_get_status(fs_tool_handle_t handle, fs_tool_status_t *status) {
    if (handle == NULL || status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    // Update space info if mounted
    if (handle->is_mounted) {
        fs_tool_update_space_info(handle);
    }
    
    status->is_initialized = handle->is_initialized;
    status->is_active = handle->is_active;
    status->is_mounted = handle->is_mounted;
    snprintf(status->mount_point, sizeof(status->mount_point), "%s", handle->config.mount_point);
    snprintf(status->partition_label, sizeof(status->partition_label), "%s", handle->config.partition_label);
    status->total_bytes = handle->total_bytes;
    status->used_bytes = handle->used_bytes;
    status->available_bytes = handle->available_bytes;
    status->usage_percent = handle->usage_percent;
    status->uptime_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - handle->uptime_start;
    status->file_operations_count = handle->file_operations_count;
    status->error_count = handle->error_count;
    status->capabilities = handle->capabilities;
    
    xSemaphoreGive(handle->mutex);
    
    return ESP_OK;
}

const fs_tool_registry_t* fs_tool_get_registry_entry(void) {
    static const fs_tool_registry_t registry = {
        .tool_id = FS_TOOL_ID,
        .version = FS_TOOL_VERSION,
        .description = FS_TOOL_DESCRIPTION,
        .capabilities = FS_CAP_MOUNT | FS_CAP_AUTO_FORMAT | FS_CAP_JSON_CONFIG |
                       FS_CAP_JSON_LOG | FS_CAP_SPACE_MONITOR | FS_CAP_EVENT_PUBLISH |
                       FS_CAP_HEALTH_CHECK | FS_CAP_ATOMIC_OPERATIONS,
        .init_func = fs_tool_init,
        .deinit_func = fs_tool_deinit,
    };
    return &registry;
}

// =============================================================================
// Filesystem Operations Implementation
// =============================================================================

esp_err_t fs_tool_mount(fs_tool_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    if (handle->is_mounted) {
        ESP_LOGW(TAG, "Filesystem already mounted");
        xSemaphoreGive(handle->mutex);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Mounting filesystem at %s", handle->config.mount_point);
    
    esp_vfs_littlefs_conf_t conf = {
        .base_path = handle->config.mount_point,
        .partition_label = strlen(handle->config.partition_label) > 0 ? handle->config.partition_label : NULL,
        .format_if_mount_failed = handle->config.format_if_mount_failed
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    
    if (ret != ESP_OK) {
        handle->error_count++;
        const char* error_msg = "Unknown error";
        
        if (ret == ESP_FAIL) {
            error_msg = "Failed to mount or format filesystem";
        } else if (ret == ESP_ERR_NOT_FOUND) {
            error_msg = "Failed to find LittleFS partition";
        } else {
            error_msg = esp_err_to_name(ret);
        }
        
        ESP_LOGE(TAG, "Mount failed: %s", error_msg);
        
        // Publish error event
        if (handle->config.publish_events) {
            fs_tool_event_t event = {
                .type = FS_TOOL_EVENT_ERROR,
                .data.error_info = {
                    .error_code = ret,
                    .error_message = error_msg
                }
            };
            strncpy(event.data.error_info.file_path, handle->config.mount_point, sizeof(event.data.error_info.file_path) - 1);
            fs_tool_publish_event(handle, FS_TOOL_EVENT_ERROR, &event, sizeof(event));
        }
        
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    handle->is_mounted = true;
    ESP_LOGI(TAG, "✅ Filesystem mounted successfully");
    
    // Update space information
    fs_tool_update_space_info(handle);
    
    // Publish mount event
    if (handle->config.publish_events) {
        fs_tool_event_t event = {
            .type = FS_TOOL_EVENT_MOUNTED,
            .data.mount_info = {
                .total_bytes = handle->total_bytes,
                .used_bytes = handle->used_bytes
            }
        };
        snprintf(event.data.mount_info.mount_point, sizeof(event.data.mount_info.mount_point), "%s", handle->config.mount_point);
        snprintf(event.data.mount_info.partition_label, sizeof(event.data.mount_info.partition_label), "%s", handle->config.partition_label);
        fs_tool_publish_event(handle, FS_TOOL_EVENT_MOUNTED, &event, sizeof(event));
    }
    
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

esp_err_t fs_tool_unmount(fs_tool_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    if (!handle->is_mounted) {
        ESP_LOGW(TAG, "Filesystem not mounted");
        xSemaphoreGive(handle->mutex);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Unmounting filesystem");
    
    esp_err_t ret = esp_vfs_littlefs_unregister(handle->config.partition_label);
    
    if (ret != ESP_OK) {
        handle->error_count++;
        ESP_LOGE(TAG, "Failed to unmount filesystem: %s", esp_err_to_name(ret));
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    handle->is_mounted = false;
    handle->total_bytes = 0;
    handle->used_bytes = 0;
    handle->available_bytes = 0;
    handle->usage_percent = 0;
    
    ESP_LOGI(TAG, "Filesystem unmounted");
    
    // Publish unmount event
    if (handle->config.publish_events) {
        fs_tool_event_t event = {
            .type = FS_TOOL_EVENT_UNMOUNTED
        };
        fs_tool_publish_event(handle, FS_TOOL_EVENT_UNMOUNTED, &event, sizeof(event));
    }
    
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

esp_err_t fs_tool_format(fs_tool_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    ESP_LOGI(TAG, "Formatting filesystem");
    
    // Publish format started event
    if (handle->config.publish_events) {
        fs_tool_event_t event = {
            .type = FS_TOOL_EVENT_FORMAT_STARTED
        };
        fs_tool_publish_event(handle, FS_TOOL_EVENT_FORMAT_STARTED, &event, sizeof(event));
    }
    
    // Unmount if mounted
    bool was_mounted = handle->is_mounted;
    if (handle->is_mounted) {
        esp_vfs_littlefs_unregister(handle->config.partition_label);
        handle->is_mounted = false;
    }
    
    esp_err_t ret = esp_littlefs_format(handle->config.partition_label);
    
    if (ret != ESP_OK) {
        handle->error_count++;
        ESP_LOGE(TAG, "Failed to format filesystem: %s", esp_err_to_name(ret));
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Filesystem formatted successfully");
    
    // Remount if it was mounted before
    if (was_mounted) {
        esp_vfs_littlefs_conf_t conf = {
            .base_path = handle->config.mount_point,
            .partition_label = strlen(handle->config.partition_label) > 0 ? handle->config.partition_label : NULL,
            .format_if_mount_failed = false  // Should not be needed after format
        };
        
        ret = esp_vfs_littlefs_register(&conf);
        if (ret == ESP_OK) {
            handle->is_mounted = true;
            fs_tool_update_space_info(handle);
        }
    }
    
    // Publish format completed event
    if (handle->config.publish_events) {
        fs_tool_event_t event = {
            .type = FS_TOOL_EVENT_FORMAT_COMPLETED
        };
        fs_tool_publish_event(handle, FS_TOOL_EVENT_FORMAT_COMPLETED, &event, sizeof(event));
    }
    
    xSemaphoreGive(handle->mutex);
    return ret;
}

bool fs_tool_is_mounted(fs_tool_handle_t handle) {
    if (handle == NULL) {
        return false;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    bool mounted = handle->is_mounted;
    xSemaphoreGive(handle->mutex);
    
    return mounted;
}

esp_err_t fs_tool_get_space_info(fs_tool_handle_t handle, uint32_t *total_bytes, uint32_t *used_bytes) {
    if (handle == NULL || total_bytes == NULL || used_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    esp_err_t ret = fs_tool_update_space_info(handle);
    
    if (ret == ESP_OK) {
        *total_bytes = handle->total_bytes;
        *used_bytes = handle->used_bytes;
    }
    
    xSemaphoreGive(handle->mutex);
    
    return ret;
}

// =============================================================================
// JSON Configuration API (For Other Tools - Key MCP Functionality)
// =============================================================================

esp_err_t fs_tool_save_json_config(fs_tool_handle_t handle, const char *filename, const cJSON *json_object) {
    if (handle == NULL || filename == NULL || json_object == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    if (!handle->is_mounted) {
        ESP_LOGE(TAG, "Filesystem not mounted");
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    char full_path[FS_TOOL_MAX_PATH_LEN];
    esp_err_t ret = fs_tool_create_full_path(handle, filename, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    char *json_string = cJSON_Print(json_object);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON to string");
        handle->error_count++;
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "💾 Saving JSON config: %s", filename);
    
    if (handle->config.use_atomic_operations) {
        ret = fs_tool_atomic_file_operation(handle, full_path, json_string, strlen(json_string), false);
    } else {
        FILE *f = fopen(full_path, "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
            ret = ESP_FAIL;
            handle->error_count++;
        } else {
            fprintf(f, "%s", json_string);
            fclose(f);
            ret = ESP_OK;
        }
    }
    
    if (ret == ESP_OK) {
        handle->file_operations_count++;
        ESP_LOGI(TAG, "✅ JSON config saved successfully: %s", filename);
        
        // Publish config saved event
        if (handle->config.publish_events) {
            size_t json_len = strlen(json_string);
            fs_tool_event_t event = {
                .type = FS_TOOL_EVENT_CONFIG_SAVED,
                .data.file_info = {
                    .file_size = json_len,
                    .success = true
                }
            };
            strncpy(event.data.file_info.file_path, filename, sizeof(event.data.file_info.file_path) - 1);
            fs_tool_publish_event(handle, FS_TOOL_EVENT_CONFIG_SAVED, &event, sizeof(event));
        }
    } else {
        handle->error_count++;
        ESP_LOGE(TAG, "Failed to save JSON config: %s", filename);
    }
    
    free(json_string);
    
    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t fs_tool_load_json_config(fs_tool_handle_t handle, const char *filename, cJSON **json_object) {
    if (handle == NULL || filename == NULL || json_object == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    if (!handle->is_mounted) {
        ESP_LOGE(TAG, "Filesystem not mounted");
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    char full_path[FS_TOOL_MAX_PATH_LEN];
    esp_err_t ret = fs_tool_create_full_path(handle, filename, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    ESP_LOGI(TAG, "📂 Loading JSON config: %s", filename);
    
    FILE *f = fopen(full_path, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "Config file not found: %s", filename);
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0) {
        ESP_LOGW(TAG, "Empty config file: %s", filename);
        fclose(f);
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Read file content
    char *buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer for config file");
        fclose(f);
        handle->error_count++;
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NO_MEM;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, f);
    fclose(f);
    
    if (bytes_read != file_size) {
        ESP_LOGE(TAG, "Failed to read complete config file");
        free(buffer);
        handle->error_count++;
        xSemaphoreGive(handle->mutex);
        return ESP_FAIL;
    }
    
    buffer[file_size] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(buffer);
    free(buffer);
    
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON config: %s", filename);
        handle->error_count++;
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_ARG;
    }
    
    *json_object = json;
    handle->file_operations_count++;
    
    ESP_LOGI(TAG, "✅ JSON config loaded successfully: %s", filename);
    
    // Publish config loaded event
    if (handle->config.publish_events) {
        fs_tool_event_t event = {
            .type = FS_TOOL_EVENT_CONFIG_LOADED,
            .data.file_info = {
                .file_size = file_size,
                .success = true
            }
        };
        strncpy(event.data.file_info.file_path, filename, sizeof(event.data.file_info.file_path) - 1);
        fs_tool_publish_event(handle, FS_TOOL_EVENT_CONFIG_LOADED, &event, sizeof(event));
    }
    
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

esp_err_t fs_tool_save_json_log(fs_tool_handle_t handle, const char *filename, const cJSON *json_object) {
    // Same implementation as save_json_config but with different event type
    if (handle == NULL || filename == NULL || json_object == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    if (!handle->is_mounted) {
        ESP_LOGE(TAG, "Filesystem not mounted");
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    char full_path[FS_TOOL_MAX_PATH_LEN];
    esp_err_t ret = fs_tool_create_full_path(handle, filename, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    char *json_string = cJSON_Print(json_object);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON to string");
        handle->error_count++;
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "📝 Saving JSON log: %s", filename);
    
    if (handle->config.use_atomic_operations) {
        ret = fs_tool_atomic_file_operation(handle, full_path, json_string, strlen(json_string), false);
    } else {
        FILE *f = fopen(full_path, "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
            ret = ESP_FAIL;
            handle->error_count++;
        } else {
            fprintf(f, "%s", json_string);
            fclose(f);
            ret = ESP_OK;
        }
    }
    
    if (ret == ESP_OK) {
        handle->file_operations_count++;
        
        // Publish log saved event
        if (handle->config.publish_events) {
            size_t json_len = strlen(json_string);
            fs_tool_event_t event = {
                .type = FS_TOOL_EVENT_LOG_SAVED,
                .data.file_info = {
                    .file_size = json_len,
                    .success = true
                }
            };
            strncpy(event.data.file_info.file_path, filename, sizeof(event.data.file_info.file_path) - 1);
            fs_tool_publish_event(handle, FS_TOOL_EVENT_LOG_SAVED, &event, sizeof(event));
        }
    } else {
        handle->error_count++;
    }
    
    free(json_string);
    
    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t fs_tool_load_json_log(fs_tool_handle_t handle, const char *filename, cJSON **json_object) {
    // Same as load_json_config but with different event type
    esp_err_t ret = fs_tool_load_json_config(handle, filename, json_object);
    
    // Override event type for logs
    if (ret == ESP_OK && handle->config.publish_events) {
        xSemaphoreTake(handle->mutex, portMAX_DELAY);
        fs_tool_event_t event = {
            .type = FS_TOOL_EVENT_LOG_LOADED,
            .data.file_info = {
                .success = true
            }
        };
        strncpy(event.data.file_info.file_path, filename, sizeof(event.data.file_info.file_path) - 1);
        fs_tool_publish_event(handle, FS_TOOL_EVENT_LOG_LOADED, &event, sizeof(event));
        xSemaphoreGive(handle->mutex);
    }
    
    return ret;
}

esp_err_t fs_tool_append_json_log(fs_tool_handle_t handle, const char *filename, const cJSON *json_entry) {
    if (handle == NULL || filename == NULL || json_entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Load existing log, append entry, save back
    cJSON *log_object = NULL;
    esp_err_t ret = fs_tool_load_json_log(handle, filename, &log_object);
    
    if (ret == ESP_ERR_NOT_FOUND) {
        // Create new log with events array
        log_object = cJSON_CreateObject();
        if (log_object == NULL) {
            return ESP_ERR_NO_MEM;
        }
        cJSON *events_array = cJSON_CreateArray();
        if (events_array == NULL) {
            cJSON_Delete(log_object);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddItemToObject(log_object, "events", events_array);
    } else if (ret != ESP_OK) {
        return ret;
    }
    
    // Get or create events array
    cJSON *events_array = cJSON_GetObjectItem(log_object, "events");
    if (!cJSON_IsArray(events_array)) {
        events_array = cJSON_CreateArray();
        if (events_array == NULL) {
            cJSON_Delete(log_object);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddItemToObject(log_object, "events", events_array);
    }
    
    // Add new entry (duplicate it to avoid ownership issues)
    cJSON *entry_copy = cJSON_Duplicate(json_entry, 1);
    if (entry_copy == NULL) {
        cJSON_Delete(log_object);
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddItemToArray(events_array, entry_copy);
    
    // Save updated log
    ret = fs_tool_save_json_log(handle, filename, log_object);
    
    cJSON_Delete(log_object);
    
    return ret;
}

// =============================================================================
// File Operations Interface
// =============================================================================

bool fs_tool_file_exists(fs_tool_handle_t handle, const char *filename) {
    if (handle == NULL || filename == NULL) {
        return false;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    if (!handle->is_mounted) {
        xSemaphoreGive(handle->mutex);
        return false;
    }
    
    char full_path[FS_TOOL_MAX_PATH_LEN];
    if (fs_tool_create_full_path(handle, filename, full_path, sizeof(full_path)) != ESP_OK) {
        xSemaphoreGive(handle->mutex);
        return false;
    }
    
    struct stat st;
    bool exists = (stat(full_path, &st) == 0);
    
    xSemaphoreGive(handle->mutex);
    return exists;
}

esp_err_t fs_tool_get_file_size(fs_tool_handle_t handle, const char *filename, uint32_t *size) {
    if (handle == NULL || filename == NULL || size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    if (!handle->is_mounted) {
        ESP_LOGE(TAG, "Filesystem not mounted");
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    char full_path[FS_TOOL_MAX_PATH_LEN];
    esp_err_t ret = fs_tool_create_full_path(handle, filename, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    struct stat st;
    if (stat(full_path, &st) != 0) {
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    *size = st.st_size;
    
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

esp_err_t fs_tool_delete_file(fs_tool_handle_t handle, const char *filename) {
    if (handle == NULL || filename == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    if (!handle->is_mounted) {
        ESP_LOGE(TAG, "Filesystem not mounted");
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    char full_path[FS_TOOL_MAX_PATH_LEN];
    esp_err_t ret = fs_tool_create_full_path(handle, filename, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    if (unlink(full_path) != 0) {
        ESP_LOGE(TAG, "Failed to delete file: %s", filename);
        handle->error_count++;
        xSemaphoreGive(handle->mutex);
        return ESP_FAIL;
    }
    
    handle->file_operations_count++;
    ESP_LOGI(TAG, "🗑️ File deleted: %s", filename);
    
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

esp_err_t fs_tool_create_directory(fs_tool_handle_t handle, const char *dirname) {
    if (handle == NULL || dirname == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    if (!handle->is_mounted) {
        ESP_LOGE(TAG, "Filesystem not mounted");
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    char full_path[FS_TOOL_MAX_PATH_LEN];
    esp_err_t ret = fs_tool_create_full_path(handle, dirname, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    if (mkdir(full_path, 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create directory: %s", dirname);
        handle->error_count++;
        xSemaphoreGive(handle->mutex);
        return ESP_FAIL;
    }
    
    handle->file_operations_count++;
    ESP_LOGI(TAG, "📁 Directory created: %s", dirname);
    
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

// =============================================================================
// Helper Functions Implementation
// =============================================================================

static esp_err_t fs_tool_update_space_info(fs_tool_handle_t handle) {
    if (!handle->is_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    size_t total_bytes = 0, used_bytes = 0;
    esp_err_t ret = esp_littlefs_info(
        strlen(handle->config.partition_label) > 0 ? handle->config.partition_label : NULL,
        &total_bytes, &used_bytes
    );
    
    if (ret == ESP_OK) {
        handle->total_bytes = total_bytes;
        handle->used_bytes = used_bytes;
        handle->available_bytes = total_bytes - used_bytes;
        
        // Safe percentage calculation with overflow protection
        if (total_bytes == 0) {
            handle->usage_percent = 0;
        } else if (used_bytes >= total_bytes) {
            handle->usage_percent = 100;  // Cap at 100% for filesystem inconsistencies
        } else {
            // Use 64-bit arithmetic to prevent overflow, then cap result
            uint64_t percentage = ((uint64_t)used_bytes * 100) / total_bytes;
            handle->usage_percent = (percentage > 100) ? 100 : (uint8_t)percentage;
        }
        
        // Check for low space warning
        if (handle->config.publish_events && 
            handle->usage_percent >= handle->config.low_space_threshold_percent) {
            fs_tool_event_t event = {
                .type = FS_TOOL_EVENT_SPACE_WARNING,
                .data.space_info = {
                    .total_bytes = handle->total_bytes,
                    .used_bytes = handle->used_bytes,
                    .available_bytes = handle->available_bytes,
                    .usage_percent = handle->usage_percent
                }
            };
            fs_tool_publish_event(handle, FS_TOOL_EVENT_SPACE_WARNING, &event, sizeof(event));
        }
    } else {
        ESP_LOGW(TAG, "Failed to get filesystem info: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

static esp_err_t fs_tool_create_full_path(fs_tool_handle_t handle, const char *filename, char *full_path, size_t max_len) {
    if (strlen(handle->config.mount_point) + strlen(filename) + 2 > max_len) {
        ESP_LOGE(TAG, "Path too long: %s/%s", handle->config.mount_point, filename);
        return ESP_ERR_INVALID_SIZE;
    }
    
    snprintf(full_path, max_len, "%s/%s", handle->config.mount_point, filename);
    return ESP_OK;
}

static esp_err_t fs_tool_atomic_file_operation(fs_tool_handle_t handle, const char *filename, const char *data, size_t data_len, bool is_append) {
    // Create temporary file for atomic operation
    char temp_filename[FS_TOOL_MAX_PATH_LEN];
    snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", filename);
    
    FILE *temp_file = fopen(temp_filename, "w");
    if (temp_file == NULL) {
        ESP_LOGE(TAG, "Failed to create temporary file: %s", temp_filename);
        return ESP_FAIL;
    }
    
    // If appending, first copy existing content
    if (is_append) {
        FILE *orig_file = fopen(filename, "r");
        if (orig_file != NULL) {
            char buffer[512];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), orig_file)) > 0) {
                fwrite(buffer, 1, bytes_read, temp_file);
            }
            fclose(orig_file);
        }
    }
    
    // Write new data
    size_t bytes_written = fwrite(data, 1, data_len, temp_file);
    fclose(temp_file);
    
    if (bytes_written != data_len) {
        ESP_LOGE(TAG, "Failed to write complete data to temporary file");
        unlink(temp_filename);
        return ESP_FAIL;
    }
    
    // Atomic rename
    if (rename(temp_filename, filename) != 0) {
        ESP_LOGE(TAG, "Failed to rename temporary file to target file");
        unlink(temp_filename);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

static void fs_tool_monitor_task(void *pvParameters) {
    fs_tool_handle_t handle = (fs_tool_handle_t)pvParameters;
    
    ESP_LOGI(TAG, "📊 Filesystem monitor task started");
    
    while (handle->is_active) {
        vTaskDelay(pdMS_TO_TICKS(handle->config.space_check_interval_ms));
        
        if (!handle->is_active) {
            break;
        }
        
        xSemaphoreTake(handle->mutex, portMAX_DELAY);
        
        if (handle->is_mounted) {
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (current_time - handle->last_space_check_ms >= handle->config.space_check_interval_ms) {
                fs_tool_update_space_info(handle);
                handle->last_space_check_ms = current_time;
            }
        }
        
        xSemaphoreGive(handle->mutex);
    }
    
    ESP_LOGI(TAG, "Filesystem monitor task ended");
    vTaskDelete(NULL);
}

static esp_err_t fs_tool_publish_event(fs_tool_handle_t handle, fs_tool_event_type_t event_type, void *event_data, size_t data_size) {
    if (!handle->config.publish_events) {
        return ESP_OK;
    }
    
    return esp_event_post(FS_TOOL_EVENTS, event_type, event_data, data_size, 0);
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* fs_tool_event_to_string(fs_tool_event_type_t event_type) {
    switch (event_type) {
        case FS_TOOL_EVENT_MOUNTED:          return "MOUNTED";
        case FS_TOOL_EVENT_UNMOUNTED:        return "UNMOUNTED";
        case FS_TOOL_EVENT_FORMAT_STARTED:   return "FORMAT_STARTED";
        case FS_TOOL_EVENT_FORMAT_COMPLETED: return "FORMAT_COMPLETED";
        case FS_TOOL_EVENT_ERROR:            return "ERROR";
        case FS_TOOL_EVENT_CONFIG_SAVED:     return "CONFIG_SAVED";
        case FS_TOOL_EVENT_CONFIG_LOADED:    return "CONFIG_LOADED";
        case FS_TOOL_EVENT_LOG_SAVED:        return "LOG_SAVED";
        case FS_TOOL_EVENT_LOG_LOADED:       return "LOG_LOADED";
        case FS_TOOL_EVENT_SPACE_WARNING:    return "SPACE_WARNING";
        default: return "UNKNOWN";
    }
}

esp_err_t fs_tool_get_full_path(fs_tool_handle_t handle, const char *filename, char *full_path, size_t max_len) {
    if (handle == NULL || filename == NULL || full_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return fs_tool_create_full_path(handle, filename, full_path, max_len);
}

esp_err_t fs_tool_health_check(fs_tool_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    // Check if filesystem is mounted
    if (!handle->is_mounted) {
        ESP_LOGW(TAG, "Health check failed: filesystem not mounted");
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Update space information
    esp_err_t ret = fs_tool_update_space_info(handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Health check failed: cannot get space info");
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    // Check if filesystem is nearly full
    if (handle->usage_percent >= 95) {
        ESP_LOGW(TAG, "Health check warning: filesystem nearly full (%d%%)", handle->usage_percent);
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Try to create and delete a test file
    char test_filename[64];
    snprintf(test_filename, sizeof(test_filename), "fs_health_test_%lu.tmp", 
             (unsigned long)(xTaskGetTickCount() & 0xFFFF));
    
    char full_path[FS_TOOL_MAX_PATH_LEN];
    ret = fs_tool_create_full_path(handle, test_filename, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->mutex);
        return ret;
    }
    
    // Create test file
    FILE *test_file = fopen(full_path, "w");
    if (test_file == NULL) {
        ESP_LOGW(TAG, "Health check failed: cannot create test file");
        xSemaphoreGive(handle->mutex);
        return ESP_FAIL;
    }
    
    fprintf(test_file, "health_check");
    fclose(test_file);
    
    // Delete test file
    if (unlink(full_path) != 0) {
        ESP_LOGW(TAG, "Health check failed: cannot delete test file");
        xSemaphoreGive(handle->mutex);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ Filesystem health check passed");
    
    xSemaphoreGive(handle->mutex);
    return ESP_OK;
}

// =============================================================================
// Event Base Definition
// =============================================================================

ESP_EVENT_DEFINE_BASE(FS_TOOL_EVENTS);