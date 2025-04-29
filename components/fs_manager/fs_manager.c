#include "fs_manager.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "fs_manager";

esp_err_t fs_manager_init(const char *mount_point, const char *partition_label, bool format_if_mount_failed) {
    ESP_LOGI(TAG, "Initializing LittleFS on %s", mount_point);
    
    esp_vfs_littlefs_conf_t conf = {
        .base_path = mount_point,
        .partition_label = partition_label,
        .format_if_mount_failed = format_if_mount_failed,
        .dont_mount = false,
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d bytes, used: %d bytes", total, used);
    }
    
    ESP_LOGI(TAG, "LittleFS mounted successfully on %s", mount_point);
    return ESP_OK;
}

esp_err_t fs_manager_deinit(const char *mount_point) {
    ESP_LOGI(TAG, "Unmounting LittleFS from %s", mount_point);
    
    esp_err_t ret = esp_vfs_littlefs_unregister(mount_point);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unregister LittleFS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "LittleFS unmounted successfully");
    return ESP_OK;
}

esp_err_t fs_manager_info(const char *partition_label, size_t *total_bytes, size_t *used_bytes) {
    if (total_bytes == NULL || used_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return esp_littlefs_info(partition_label, total_bytes, used_bytes);
}