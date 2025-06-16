# wifi_tool - MCP WiFi Connectivity Tool

## Overview

The `wifi_tool` is a self-contained MCP-style tool providing WiFi connectivity management for ESP32 devices. It supports both Station (STA) and Access Point (AP) modes with multi-network configurations, automatic reconnection, and event-driven communication.

## Key Features

- **MCP Architecture**: Handle-based lifecycle, event publishing, capabilities discovery
- **Multi-Network Support**: Configure multiple WiFi networks with priority ordering
- **Automatic Connection**: Auto-connect on startup using stored credentials
- **AP Mode Fallback**: Start AP mode for configuration when STA connection fails
- **Event-Driven**: Publishes WiFi events for other tools (feedback, webhook, etc.)
- **JSON Configuration**: Load network credentials via fs_tool integration
- **Self-Contained**: No external tool dependencies (only system ESP-IDF headers)

## Tool Dependencies

**This tool provides APIs that other tools depend on:**
- `main.c` ’ wifi_tool APIs for network connectivity
- `webhook_tool` ’ WiFi events for transmission triggers
- `feedback_tool` ’ WiFi events for visual status indication

**Dependency Hierarchy:**
```
Higher-level tools (webhook_tool, feedback_tool, etc.)
    “ (subscribe to WiFi events)
wifi_tool 
    “ (loads config via)
fs_tool APIs (dependency injection)
    “ (uses)
ESP32 WiFi/networking system
```

## Integration Guide

### 1. Tool Initialization

```c
#include "wifi_tool.h"

// Create configuration
wifi_tool_config_t wifi_config = wifi_tool_create_default_config();
// Customize if needed:
// wifi_config.auto_reconnect = true;
// wifi_config.enable_ap_fallback = true;

// Initialize tool
wifi_tool_handle_t wifi_tool = wifi_tool_init(&wifi_config);
if (wifi_tool == NULL) {
    ESP_LOGE(TAG, "Failed to initialize wifi_tool");
    return;
}
```

### 2. Configuration Loading (MCP Pattern)

```c
// PHASE 4.2: Proper MCP dependency injection pattern
// Step 1: Set filesystem dependency
esp_err_t ret = wifi_tool_set_fs_dependency(wifi_tool, fs_tool_handle);

// Step 2: Load configuration via main.c orchestration (NOT directly in wifi_tool)
cJSON *wifi_config = NULL;
ret = fs_tool_load_json_config(fs_tool_handle, "wifi.json", &wifi_config);

// Step 3: Pass parsed config to wifi_tool
ret = wifi_tool_load_networks_from_json(wifi_tool, wifi_config);
cJSON_Delete(wifi_config);

// Step 4: Start automatic connection
ret = wifi_tool_start_auto_connection(wifi_tool);
```

### 3. WiFi Configuration Format (wifi.json)

```json
{
  "networks": [
    {
      "ssid": "MyNetwork",
      "password": "MyPassword"
    },
    {
      "ssid": "BackupNetwork", 
      "password": "BackupPassword"
    }
  ]
}
```

### 4. Event Subscription Pattern

```c
// Subscribe to WiFi events for coordination
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != WIFI_TOOL_EVENTS) return;
    
    wifi_tool_event_t* wifi_event = (wifi_tool_event_t*)event_data;
    
    switch (event_id) {
        case WIFI_TOOL_EVENT_STA_CONNECTING:
            ESP_LOGI(TAG, "WiFi connecting to: %s", wifi_event->data.sta_info.ssid);
            break;
        case WIFI_TOOL_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected to: %s", wifi_event->data.sta_info.ssid);
            break;
        case WIFI_TOOL_EVENT_IP_ACQUIRED:
            ESP_LOGI(TAG, "IP acquired: %s", wifi_event->data.ip_info.ip_address);
            break;
        case WIFI_TOOL_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected");
            break;
    }
}

esp_event_handler_register(WIFI_TOOL_EVENTS, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
```

### 5. Manual WiFi Operations

```c
// Manual network connection
esp_err_t ret = wifi_tool_connect(wifi_tool, "MySSID", "MyPassword");

// Start AP mode manually
ret = wifi_tool_start_ap(wifi_tool);

// Check connection status
bool is_connected = wifi_tool_is_connected(wifi_tool);

// Get IP address
char ip_str[16];
ret = wifi_tool_get_ip_address(wifi_tool, ip_str, sizeof(ip_str));
```

### 6. Tool Status Monitoring

```c
wifi_tool_status_t status;
esp_err_t ret = wifi_tool_get_status(wifi_tool, &status);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "WiFi: %s, AP: %s, SSID: %s, IP: %s", 
             status.sta_connected ? "Connected" : "Disconnected",
             status.ap_active ? "Active" : "Inactive",
             status.current_ssid,
             status.ip_address);
}
```

## MCP Pattern Implementation

### Handle-Based Design
```c
//  Proper MCP pattern - no static globals
wifi_tool_handle_t wifi_tool = wifi_tool_init(&config);
esp_err_t result = wifi_tool_start_auto_connection(wifi_tool);
wifi_tool_deinit(wifi_tool);
```

### Capabilities Discovery
```c
wifi_tool_capabilities_t caps = wifi_tool_get_capabilities(wifi_tool);
if (caps & WIFI_CAP_STA_MODE) {
    // Tool supports Station mode
}
if (caps & WIFI_CAP_AP_MODE) {
    // Tool supports Access Point mode
}
if (caps & WIFI_CAP_AUTO_RECONNECT) {
    // Tool supports automatic reconnection
}
```

### Tool Registry Integration
```c
const wifi_tool_registry_t* registry = wifi_tool_get_registry_entry();
ESP_LOGI(TAG, "Tool: %s v%s - %s", 
         registry->tool_id, 
         registry->version, 
         registry->description);
```

## Event Types Published

The wifi_tool publishes these events for other tools:

- `WIFI_TOOL_EVENT_STA_CONNECTING`: Connection attempt started
- `WIFI_TOOL_EVENT_STA_CONNECTED`: Successfully connected to network
- `WIFI_TOOL_EVENT_STA_DISCONNECTED`: Disconnected from network
- `WIFI_TOOL_EVENT_STA_FAILED`: Connection attempt failed
- `WIFI_TOOL_EVENT_AP_STARTED`: AP mode started
- `WIFI_TOOL_EVENT_AP_STOPPED`: AP mode stopped
- `WIFI_TOOL_EVENT_IP_ACQUIRED`: IP address obtained
- `WIFI_TOOL_EVENT_AP_CLIENT_CONNECTED`: Client connected to our AP
- `WIFI_TOOL_EVENT_AP_CLIENT_DISCONNECTED`: Client disconnected from our AP

## Configuration Options

The tool can be configured via Kconfig:
- WiFi connection timeouts and retry counts
- AP mode settings (SSID, password, channel)
- Event publishing options

## Breaking Coupling Violations

### L Wrong Way (Direct ESP-IDF calls)
```c
// Violates MCP self-containment
esp_wifi_init(&cfg);
esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
esp_wifi_connect();
```

###  Right Way (wifi_tool APIs)
```c
// Proper MCP pattern
wifi_tool_handle_t wifi = wifi_tool_init(&config);
wifi_tool_set_fs_dependency(wifi, fs_tool);
wifi_tool_load_networks_from_json(wifi, json_config);
wifi_tool_start_auto_connection(wifi);
wifi_tool_deinit(wifi);
```

## Hardware Requirements

- ESP32-C3 or compatible with WiFi support
- Network credentials stored in LittleFS (wifi.json)
- Event loop for publishing WiFi events

## Critical Architecture Lessons

### MCP Dependency Injection Pattern
** CORRECT**: Tools never include other tool headers directly
```c
// wifi_tool.h - Forward declaration only
typedef struct fs_tool_context* fs_tool_handle_t;
esp_err_t wifi_tool_set_fs_dependency(wifi_tool_handle_t, fs_tool_handle_t);

// wifi_tool.c - NO #include "fs_tool.h" needed!
// Main.c orchestrates between tools
```

### Event-Driven Architecture
** CORRECT**: WiFi state changes trigger events, other tools subscribe
- `feedback_tool` subscribes for visual status indication
- `webhook_tool` subscribes for transmission triggers
- Perfect decoupling - no direct function calls between tools

## Error Handling

The tool provides comprehensive error handling:
- `ESP_ERR_INVALID_ARG`: Invalid parameters
- `ESP_ERR_INVALID_STATE`: Tool not initialized
- `ESP_ERR_NOT_FOUND`: No networks configured
- `ESP_ERR_WIFI_*`: WiFi-specific errors from ESP-IDF

## Hardware Validation

To validate wifi_tool integration:

1. **Initialize Tool**: Verify WiFi tool starts successfully
2. **Config Loading**: Test JSON config load via fs_tool dependency injection
3. **Auto Connection**: Verify automatic connection to configured networks
4. **Event Publishing**: Test event coordination with other tools (feedback, webhook)
5. **AP Mode**: Test fallback AP mode when STA connection fails
6. **Status Monitoring**: Verify tool status reporting

## Best Practices

1. **Always use dependency injection** for fs_tool integration
2. **Subscribe to events** instead of polling WiFi status
3. **Handle connection failures** gracefully with retries or AP fallback
4. **Use main.c orchestration** for loading config between tools
5. **Never include other tool headers** - use forward declarations
6. **Test both STA and AP modes** for complete validation

This tool is essential for network connectivity in the MCP time tracking ecosystem and demonstrates proper tool isolation with event-driven coordination.