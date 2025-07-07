# ntp_tool - MCP NTP Time Synchronization Tool

## Overview

The `ntp_tool` is a self-contained MCP-style tool providing NTP time synchronization for ESP32 devices. It features WiFi-triggered synchronization, automatic periodic sync, and event-driven communication following established MCP architecture patterns.

## Key Features

- **MCP Architecture**: Handle-based lifecycle, event publishing, capabilities discovery
- **WiFi-Triggered Sync**: Automatically synchronizes time when WiFi connects
- **Multiple NTP Servers**: Configurable server list with priority ordering
- **Automatic Sync**: Periodic time synchronization with configurable intervals
- **Event-Driven**: Publishes time events for other tools (webhook, logging, etc.)
- **Timezone Support**: Configurable timezone with automatic DST handling
- **Self-Contained**: No external tool dependencies (only system ESP-IDF headers)

## Tool Dependencies

**This tool provides APIs that other tools depend on:**
- `webhook_tool` → Accurate timestamps for event transmission
- `rfid_tool` → Precise timing for session tracking
- `fs_tool` → Accurate file timestamps

**Dependency Hierarchy:**
```
Higher-level tools (webhook_tool, rfid_tool, etc.)
    ↓ (subscribe to time events)
ntp_tool 
    ↓ (triggers sync on WiFi connection)
WiFi connection events
    ↓ (uses)
ESP32 SNTP/networking system
```

## Integration Guide

### 1. Tool Initialization

```c
#include "ntp_tool.h"

// Create configuration
ntp_tool_config_t ntp_config = ntp_tool_create_default_config();
// Customize if needed:
// ntp_config.sync_interval_s = 1800; // 30 minutes
// ntp_config.wifi_triggered_sync = true;

// Initialize tool
ntp_tool_handle_t ntp_tool = ntp_tool_init(&ntp_config);
if (ntp_tool == NULL) {
    ESP_LOGE(TAG, "Failed to initialize ntp_tool");
    return;
}
```

### 2. Event Subscription Pattern

```c
// Subscribe to NTP events for timestamping
static void ntp_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != NTP_TOOL_EVENTS) return;
    
    ntp_tool_event_t* ntp_event = (ntp_tool_event_t*)event_data;
    
    switch (event_id) {
        case NTP_TOOL_EVENT_SYNC_STARTED:
            ESP_LOGI(TAG, "NTP sync started");
            break;
        case NTP_TOOL_EVENT_SYNC_SUCCESS:
            ESP_LOGI(TAG, "Time synchronized successfully");
            // Time is now accurate - safe to generate timestamps
            break;
        case NTP_TOOL_EVENT_SYNC_FAILED:
            ESP_LOGW(TAG, "NTP sync failed");
            break;
        case NTP_TOOL_EVENT_TIME_UPDATED:
            ESP_LOGI(TAG, "System time updated, offset: %" PRId64 " μs", 
                     ntp_event->data.time_info.offset_us);
            break;
    }
}

esp_event_handler_register(NTP_TOOL_EVENTS, ESP_EVENT_ANY_ID, ntp_event_handler, NULL);
```

### 3. Manual Time Operations

```c
// Manual sync trigger
esp_err_t ret = ntp_tool_sync_now(ntp_tool);

// Get current timestamp
time_t current_time;
ret = ntp_tool_get_timestamp(ntp_tool, &current_time);

// Format timestamp for display
char time_str[64];
ret = ntp_tool_format_time(current_time, time_str, sizeof(time_str));
ESP_LOGI(TAG, "Current time: %s", time_str);
```

### 4. Tool Status Monitoring

```c
ntp_tool_status_t status;
esp_err_t ret = ntp_tool_get_status(ntp_tool, &status);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "NTP Status: %s, Last sync: %" PRId64 ", Successful: %" PRIu32 "/%" PRIu32, 
             ntp_tool_status_to_string(status.sync_status),
             (int64_t)status.last_sync_time,
             status.successful_syncs,
             status.sync_attempts);
}
```

## MCP Pattern Implementation

### Handle-Based Design
```c
// Proper MCP pattern - no static globals
ntp_tool_handle_t ntp_tool = ntp_tool_init(&config);
esp_err_t result = ntp_tool_sync_now(ntp_tool);
ntp_tool_deinit(ntp_tool);
```

### Capabilities Discovery
```c
ntp_tool_capabilities_t caps = ntp_tool_get_capabilities(ntp_tool);
if (caps & NTP_CAP_WIFI_TRIGGERED) {
    // Tool supports WiFi-triggered sync
}
if (caps & NTP_CAP_AUTO_SYNC) {
    // Tool supports automatic periodic sync
}
```

### Tool Registry Integration
```c
const ntp_tool_registry_t* registry = ntp_tool_get_registry_entry();
ESP_LOGI(TAG, "Tool: %s v%s - %s", 
         registry->tool_id, 
         registry->version, 
         registry->description);
```

## Event Types Published

The ntp_tool publishes these events for other tools:

- `NTP_TOOL_EVENT_SYNC_STARTED`: NTP synchronization started
- `NTP_TOOL_EVENT_SYNC_SUCCESS`: Time synchronized successfully
- `NTP_TOOL_EVENT_SYNC_FAILED`: Synchronization failed
- `NTP_TOOL_EVENT_TIMEZONE_CHANGED`: Timezone configuration changed
- `NTP_TOOL_EVENT_TIME_UPDATED`: System time updated

## Configuration Options

The tool can be configured via Kconfig:
- Default sync interval and timeout
- Maximum retry attempts and delays
- Default timezone setting
- Debug logging options

## Integration with Other Tools

### Webhook Tool Integration
```c
// In webhook_tool event handler
case NTP_TOOL_EVENT_SYNC_SUCCESS:
    // Time is now accurate - send any pending events with correct timestamps
    webhook_tool_process_pending_events(webhook_tool);
    break;
```

### RFID Tool Integration
```c
// In RFID tool - get accurate timestamp for session tracking
time_t session_start;
ntp_tool_get_timestamp(ntp_tool, &session_start);

// Use in RFID event
rfid_event_t event = {
    .timestamp = session_start,
    .tag_uid = tag_uid,
    .event_type = RFID_TAG_DETECTED
};
```

## Hardware Requirements

- ESP32-C3 or compatible with WiFi support
- Internet connectivity for NTP server access
- Event loop for publishing NTP events

## Critical Architecture Lessons

### WiFi-Triggered Synchronization
**CORRECT**: NTP tool subscribes to WiFi connection events
```c
// WiFi event triggers immediate sync
case WIFI_TOOL_EVENT_IP_ACQUIRED:
    ESP_LOGI(TAG, "WiFi connected - triggering NTP sync");
    ntp_tool_sync_now(ntp_tool);
    break;
```

### Event-Driven Time Updates
**CORRECT**: Other tools subscribe to NTP events for accurate timestamps
- `webhook_tool` waits for sync success before sending events
- `rfid_tool` uses NTP timestamps for session tracking
- Perfect decoupling - no direct function calls between tools

## Error Handling

The tool provides comprehensive error handling:
- `ESP_ERR_INVALID_ARG`: Invalid parameters
- `ESP_ERR_INVALID_STATE`: Tool not initialized
- `ESP_ERR_TIMEOUT`: NTP sync timeout
- `ESP_ERR_NOT_FOUND`: No NTP servers configured

## Hardware Validation

To validate ntp_tool integration:

1. **Initialize Tool**: Verify NTP tool starts successfully
2. **WiFi Trigger**: Test automatic sync on WiFi connection
3. **Manual Sync**: Verify manual sync operations
4. **Event Publishing**: Test event coordination with other tools
5. **Timezone**: Test timezone configuration and DST handling
6. **Status Monitoring**: Verify tool status reporting

## Best Practices

1. **Always wait for sync success** before generating critical timestamps
2. **Subscribe to NTP events** instead of polling sync status
3. **Configure multiple NTP servers** for redundancy
4. **Use WiFi-triggered sync** for immediate time accuracy
5. **Monitor sync health** and handle failures gracefully
6. **Test timezone changes** especially around DST transitions

## Default Configuration

The tool ships with sensible defaults:
- **NTP Servers**: pool.ntp.org, time.nist.gov, time.google.com
- **Sync Interval**: 1 hour (3600 seconds)
- **Timeout**: 10 seconds
- **Timezone**: UTC
- **WiFi-Triggered**: Enabled
- **Auto-Sync**: Enabled

This tool provides essential time synchronization for the MCP time tracking ecosystem and enables accurate timestamps for all RFID events and webhook transmissions.