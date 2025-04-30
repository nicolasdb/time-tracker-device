# Time Tracker Device - Implementation Notes

This document outlines important decisions, approaches, and conventions used in the Time Tracker Device project to ensure reliability, maintainability, and reproducibility across devices and future projects.

## Device Identification

### Device UID Generation

We generate a unique device identifier by using the ESP32's embedded efuse MAC address:

```c
uint8_t chipid[6];
esp_efuse_mac_get_default(chipid);
unsigned int unique_id = ((unsigned int)chipid[0] << 16) | 
                         ((unsigned int)chipid[1] << 8) | chipid[2];
char device_id_str[20];
snprintf(device_id_str, sizeof(device_id_str), "NFC_%06X", unique_id);
```

**Key considerations:**

1. **Reliability**: MAC address is burned into efuse during manufacturing, making it consistent even after firmware updates
2. **Format**: Using "NFC_XXXXXX" format (6 hex digits) for human readability and system identification
3. **Consistency**: Takes only the first 3 bytes of MAC for compatibility with device inventory systems
4. **Cross-platform**: Implementation uses ESP-IDF native functions rather than Arduino-specific APIs

For future projects, maintain this approach to ensure inventory tracking consistency.

## NTP Time Synchronization

### Implementation Approach

We use the following principles for NTP synchronization:

1. **Async operation**: Time synchronization happens in the background
2. **Callback-based**: Uses notification callbacks instead of blocking operations
3. **Error handling**: Checks if SNTP is already running before initialization
4. **Timezone management**: Supports Brussels timezone (CET) with automatic DST detection

Implementation pattern to follow in future projects:

```c
// Check if already running
if (esp_sntp_enabled()) {
    // Skip initialization
} else {
    // Configure first
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER);
    esp_sntp_set_time_sync_notification_cb(callback_function);
    
    // Then initialize
    esp_sntp_init();
}
```

### DST Calculation

We implement European DST rules (last Sunday of March to last Sunday of October) for the Brussels timezone.

## Project Component Structure

### ESP-IDF Component-Based Architecture

We follow ESP-IDF's component-based architecture:

1. Each functionality lives in its own component directory
2. Components expose clear API through header files
3. Implementation details are hidden in C files
4. Dependencies between components are explicit

For example, our WiFi Manager component:
- Header: `components/wifi_manager/wifi_manager.h` 
- Implementation: `components/wifi_manager/wifi_manager.c`
- Dependencies: esp_wifi, esp_netif, json, ap_webserver, lwip

## Coding Standards and Conventions

### Error Handling

We use ESP-IDF's error handling pattern consistently:

```c
esp_err_t result = function_call();
if (result != ESP_OK) {
    ESP_LOGE(TAG, "Operation failed: %s", esp_err_to_name(result));
    // Handle error or return
}
```

### Logging

Consistent logging pattern:
- `ESP_LOGE`: Errors that prevent normal operation
- `ESP_LOGW`: Warnings about unexpected but recoverable situations
- `ESP_LOGI`: Informational messages about normal operation
- `ESP_LOGD`: Debug messages for developers
- `ESP_LOGV`: Verbose debug messages

### Memory Management

Always:
- Check buffer sizes before operations
- Use appropriate data types (unsigned int vs uint32_t with format specifiers)
- Avoid dynamic memory allocation where possible

## Status Display Pattern

For monitoring device operation, we use a standard status display format:

```
=================== STATUS UPDATE ===================
WiFi: Connected | IP: 192.168.1.26 | RSSI: -69 dBm
Time: Synchronized | Local time: 2025-04-30 12:34:56
Device: ID: NFC_F0F5BD | Free heap: 215216 bytes
====================================================
```

This standardized display makes it easier to understand device state at a glance.
