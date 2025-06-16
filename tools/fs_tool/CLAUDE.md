# fs_tool - MCP Filesystem Tool

## Overview

The `fs_tool` is a self-contained MCP-style tool that provides persistent storage management for ESP32-C3 devices. It embeds the `esp_littlefs` component and offers JSON configuration/log APIs for other tools, eliminating direct filesystem coupling.

## Key Features

- **MCP Architecture**: Handle-based lifecycle, event publishing, capabilities discovery
- **JSON APIs**: Simplified config/log storage for other tools
- **Self-Contained**: Embedded esp_littlefs component, no external dependencies
- **Space Monitoring**: Automatic disk usage tracking with low-space warnings
- **Atomic Operations**: Safe file operations with temporary files and atomic rename
- **Health Monitoring**: Periodic filesystem health checks

## Tool Dependencies

**This tool provides APIs that other tools depend on:**
- `webhook_tool` → fs_tool APIs for persistent storage
- `wifi_tool` → fs_tool APIs for configuration storage
- Any tool requiring persistent JSON storage

**Dependency Hierarchy:**
```
Higher-level tools (webhook_tool, wifi_tool, etc.)
    ↓ (use fs_tool APIs)
fs_tool 
    ↓ (embeds)
esp_littlefs component
    ↓ (uses)
ESP32 partition system
```

## Integration Guide

### 1. Tool Initialization

```c
#include "fs_tool.h"

// Create configuration
fs_tool_config_t fs_config = fs_tool_create_default_config();
// Customize if needed:
// strcpy(fs_config.mount_point, "/data");
// fs_config.auto_mount_on_init = true;

// Initialize tool
fs_tool_handle_t fs_tool = fs_tool_init(&fs_config);
if (fs_tool == NULL) {
    ESP_LOGE(TAG, "Failed to initialize fs_tool");
    return;
}
```

### 2. JSON Configuration Storage (For Other Tools)

```c
// Save tool configuration
cJSON *config_json = cJSON_CreateObject();
cJSON_AddStringToObject(config_json, "webhook_url", "https://api.example.com/webhook");
cJSON_AddNumberToObject(config_json, "max_retries", 3);

esp_err_t ret = fs_tool_save_json_config(fs_tool, "webhook_config.json", config_json);
cJSON_Delete(config_json);

// Load tool configuration
cJSON *loaded_config = NULL;
ret = fs_tool_load_json_config(fs_tool, "webhook_config.json", &loaded_config);
if (ret == ESP_OK) {
    // Use loaded_config...
    cJSON_Delete(loaded_config);
}
```

### 3. JSON Log Storage

```c
// Create log entry
cJSON *log_entry = cJSON_CreateObject();
cJSON_AddStringToObject(log_entry, "event_type", "tag_detected");
cJSON_AddStringToObject(log_entry, "tag_uid", "A1B2C3D4");
cJSON_AddNumberToObject(log_entry, "timestamp", time(NULL));

// Append to log file
esp_err_t ret = fs_tool_append_json_log(fs_tool, "events.json", log_entry);
cJSON_Delete(log_entry);
```

### 4. Event Subscription

```c
// Subscribe to filesystem events
static void fs_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    fs_tool_event_t* fs_event = (fs_tool_event_t*)event_data;
    
    switch (event_id) {
        case FS_TOOL_EVENT_MOUNTED:
            ESP_LOGI(TAG, "Filesystem mounted: %s", fs_event->data.mount_info.mount_point);
            break;
        case FS_TOOL_EVENT_SPACE_WARNING:
            ESP_LOGW(TAG, "Low disk space: %d%% used", fs_event->data.space_info.usage_percent);
            break;
    }
}

esp_event_handler_instance_register(FS_TOOL_EVENTS, ESP_EVENT_ANY_ID, fs_event_handler, NULL, NULL);
```

### 5. Tool Status Monitoring

```c
fs_tool_status_t status;
esp_err_t ret = fs_tool_get_status(fs_tool, &status);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Filesystem: %s, Usage: %d%%, Operations: %lu", 
             status.is_mounted ? "mounted" : "unmounted",
             status.usage_percent,
             status.file_operations_count);
}
```

## MCP Pattern Implementation

### Handle-Based Design
```c
// ✅ Proper MCP pattern - no static globals
fs_tool_handle_t fs_tool = fs_tool_init(&config);
esp_err_t result = fs_tool_save_json_config(fs_tool, filename, json);
fs_tool_deinit(fs_tool);
```

### Capabilities Discovery
```c
fs_tool_capabilities_t caps = fs_tool_get_capabilities(fs_tool);
if (caps & FS_CAP_JSON_CONFIG) {
    // Tool supports JSON configuration storage
}
if (caps & FS_CAP_ATOMIC_OPERATIONS) {
    // Tool supports atomic file operations
}
```

### Tool Registry Integration
```c
const fs_tool_registry_t* registry = fs_tool_get_registry_entry();
ESP_LOGI(TAG, "Tool: %s v%s - %s", 
         registry->tool_id, 
         registry->version, 
         registry->description);
```

## Configuration Options

The tool can be configured via Kconfig:
- `FS_TOOL_DEFAULT_MOUNT_POINT`: Mount point path (default: "/littlefs")
- `FS_TOOL_AUTO_FORMAT`: Auto-format on mount failure (default: enabled)
- `FS_TOOL_SPACE_CHECK_INTERVAL_MS`: Space monitoring interval (default: 30s)
- `FS_TOOL_LOW_SPACE_THRESHOLD`: Low space warning percentage (default: 85%)

## Breaking Coupling Violations

### ❌ Wrong Way (Direct filesystem calls)
```c
// Violates MCP self-containment
FILE *f = fopen("/littlefs/config.json", "w");
fprintf(f, "{\"url\":\"https://example.com\"}");
fclose(f);
```

### ✅ Right Way (fs_tool APIs)
```c
// Proper MCP pattern
fs_tool_handle_t fs = fs_tool_init(&fs_config);
cJSON *config = cJSON_CreateObject();
cJSON_AddStringToObject(config, "url", "https://example.com");
fs_tool_save_json_config(fs, "config.json", config);
cJSON_Delete(config);
fs_tool_deinit(fs);
```

## Partition Requirements

The fs_tool requires a LittleFS partition. The recommended partition table for Phase 3C+:

```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 2M,        # Expanded for full tool ecosystem
littlefs, data, spiffs,  0x210000, 1536K,    # Expanded for configs and logs
```

## Error Handling

The tool provides comprehensive error handling:
- `ESP_ERR_INVALID_ARG`: Invalid parameters
- `ESP_ERR_INVALID_STATE`: Filesystem not mounted
- `ESP_ERR_NOT_FOUND`: File/config not found
- `ESP_ERR_NO_MEM`: Insufficient memory or disk space
- `ESP_FAIL`: General filesystem operation failure

## Thread Safety

All fs_tool operations are thread-safe through mutex protection. Multiple tools can safely use fs_tool APIs concurrently.

## Hardware Validation

To validate fs_tool integration:

1. **Initialize Tool**: Verify filesystem mounts successfully
2. **Config Storage**: Test JSON config save/load cycle
3. **Log Storage**: Test JSON log append operations
4. **Space Monitoring**: Verify space check events
5. **Health Check**: Run filesystem health validation
6. **Multi-Tool**: Test with other tools using fs_tool APIs

## Best Practices

1. **Always check return values** from fs_tool APIs
2. **Use JSON APIs** instead of direct file operations
3. **Subscribe to events** for filesystem status monitoring
4. **Handle mount failures** gracefully in your tool
5. **Free cJSON objects** after loading configurations
6. **Use relative filenames** (fs_tool handles mount point internally)

This tool is essential for maintaining MCP architecture principles and eliminating filesystem coupling violations across the entire tool ecosystem.