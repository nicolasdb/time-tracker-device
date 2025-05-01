# Time Tracker ESP32-C3 Project

## Project Overview

This project implements an RFID-based time tracking system using the ESP32-C3 microcontroller. It reads RFID/NFC tags and sends tag placement/removal events to a remote server via webhooks.

## Features

- RFID/NFC tag reading (RC522 support with PN532 compatibility)
- WiFi connectivity with multi-SSID support
- Configuration via web interface (AP mode)
- NTP time synchronization
- Webhook integration for tracking events
- Status LED indicators
- LittleFS for configuration storage

## Project Structure

The project follows ESP-IDF component-based architecture:

- `main/`: Entry point and application orchestration
- `components/`: Module-specific implementation
  - `fs_manager/`: LittleFS implementation
  - `wifi_manager/`: WiFi connectivity and NTP
  - `rfid/`: RFID module interfaces (RC522/PN532)
  - `webhook_manager/`: Event transmission to server
  - Others: Additional functionality

## Implementation Status

| Phase | Feature | Status |
|-------|---------|--------|
| 0 | Base ESP-IDF Project | ✅ |
| 1 | LittleFS Integration | ✅ |
| 2 | WiFi JSON Parser | ✅ |
| 3 | AP + Web Config | ✅ |
| 4 | Multi-SSID WiFi | ✅ |
| 5 | NTP Sync | ✅ |
| 6 | RFID Reading | ✅ |
| 7 | Webhook Integration | ✅ |
| 8 | LED Feedback | 🔄 |

## Configuration

### WiFi Setup

Create a `wifi.json` file in the `/data` directory:

```json
{
  "networks": [
    { "ssid": "YourWiFi", "password": "YourPassword" },
    { "ssid": "BackupWiFi", "password": "BackupPassword" }
  ]
}
```

### Webhook Configuration

The webhook settings are configured at compile time using ESP-IDF's Kconfig system:

```bash
# View configuration options
idf.py menuconfig
# Navigate to: Time Tracker Configuration → Webhook Manager Configuration
```

Available settings:
- **Webhook endpoint URL**: Target URL for webhook events
- **Maximum retry attempts**: Number of retries for failed transmissions
- **Retry delay**: Time between retry attempts
- **Maximum log entries**: Number of events to store in memory
- **Debug logs**: Enable extra debugging information

For quick configuration, you can also edit the sdkconfig.defaults.webhook file and run:
```bash
idf.py fullclean
idf.py build
```

**Note for future development**: A dual-layer configuration approach is planned:
1. Compile-time configuration via Kconfig (current method)
2. Runtime configuration via LittleFS JSON file that can override defaults

This approach will provide flexibility for both development and deployment scenarios, allowing configuration changes without recompiling.

## Building and Flashing

### Prerequisites

- ESP-IDF v5.x or newer
- Python 3.7 or newer
- Required hardware modules (ESP32-C3, RC522/PN532, NeoPixel)

## Event Format

The webhook payload sent when a tag is detected:

```json
{
  "event": "tag_placed",
  "tag_uid": "04B78FB0790000",
  "device_id": "NFC_F0F5BD",
  "timestamp": "2025-04-30T19:42:51+0200",
  "tag_type": "MIFARE_UL",
  "firmware_version": "v1.0.0",
  "hardware": "ESP32-C3"
}
```

## Documentation

Additional documentation:

- [Mission Document](mission.md)
- [Webhook Integration](webhook_integration_complete.md)
