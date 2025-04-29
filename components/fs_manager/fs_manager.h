#ifndef FS_MANAGER_H
#define FS_MANAGER_H

#include "esp_err.h"
#include "esp_littlefs.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Wrapper for esp_vfs_littlefs_register
 * 
 * @param mount_point Mount point for the filesystem
 * @param partition_label Partition label. Set to NULL for default partition
 * @param format_if_mount_failed Format the filesystem if mount fails
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_manager_init(const char *mount_point, const char *partition_label, bool format_if_mount_failed);

/**
 * @brief Wrapper for esp_vfs_littlefs_unregister
 * 
 * @param mount_point Mount point to unregister
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_manager_deinit(const char *mount_point);

/**
 * @brief Get filesystem information
 * 
 * @param partition_label Partition label
 * @param total_bytes Pointer to total bytes
 * @param used_bytes Pointer to used bytes
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fs_manager_info(const char *partition_label, size_t *total_bytes, size_t *used_bytes);

#endif /* FS_MANAGER_H */