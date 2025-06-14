# ESP32-C3 Time Tracker Device

## What It Does

A smart RFID time tracking device that automatically records when you place or remove work tags. Simply place your RFID tag on the device - it lights up green and start to log your session to your time tracking system.

## Key Benefits

**For Users:**

- **Effortless tracking**: Just touch your tag - no apps, no buttons, no manual timers
- **Visual feedback**: LED shows connection status and session state  
- **Offline resilience**: Stores events when WiFi is down, syncs when reconnected
- **Zero maintenance**: Runs 24/7, auto-updates time, handles network changes

**For Developers:**

- **Reusable architecture**: Tool-based components work across different ESP32 projects
- **MCP-inspired design**: Modular tools communicate via events, not tight coupling
- **Easy customization**: Swap RFID for buttons, webhooks for MQTT, LEDs for displays
- **Battle-tested**: Production-ready with retry logic, error handling, and persistence

## Architecture Vision: MCP-Inspired Tools

This device demonstrates **tool-based architecture** inspired by Anthropic's Model Context Protocol (MCP). Instead of monolithic code, the system composes independent, reusable tools:

```txt
🎯 main.c (Pure Orchestrator)
├── 🔧 rfid_tool        → Publishes tag events
├── 🔧 wifi_tool        → Publishes connectivity status  
├── 🔧 webhook_tool     → Subscribes to events, sends HTTP
├── 🔧 feedback_tool    → Subscribes to all, shows LED patterns
└── 🔧 webserver_tool   → Handles WiFi configuration
```

**Benefits of Tool Architecture:**

- **Reusability**: Use `rfid_tool` in door access, `webhook_tool` in IoT sensors
- **Testability**: Each tool tests in isolation with mocked dependencies
- **Maintainability**: Clear boundaries, single responsibility per tool
- **Extensibility**: Add `mqtt_tool`, `display_tool`, `button_tool` without touching existing code

This makes the ESP32 ecosystem more like modern development - composable, testable, reusable components instead of copy-paste spaghetti code.

## Quick Start

1. **Hardware**: Connect ESP32-C3 + RC522 RFID reader + WS2812 LED
2. **WiFi Setup**: Device creates `TimeTracker-AP` → connect → browse to `192.168.4.1` → configure
3. **Webhook**: Set your time tracking endpoint via `idf.py menuconfig`
4. **Use**: Touch RFID tag → green LED → automatic time logging

## Technical Details

**Hardware Requirements:**

- ESP32-C3 development board
- MFRC522 RFID reader module  
- WS2812B addressable LED (optional visual feedback)

**Configuration:**

- WiFi: Web interface at `192.168.4.1` when in AP mode
- Webhook: ESP-IDF menuconfig → Time Tracker Configuration
- Advanced: See `CLAUDE.md` for developer instructions

**Generic Event Format:**

```json
{
  "event": "tag_placed",
  "tag_uid": "04B78FB0790000", 
  "device_id": "ESP32_F0F5BD",
  "timestamp": "2025-06-14T10:30:45+01:00"
}
```

## Development

This project is undergoing an **MCP-inspired architectural refactor** to transform tightly-coupled components into reusable, testable tools.

**Current Status:** Functional prototype with some architectural debt  
**Target:** Clean tool-based architecture with full test coverage

See `refactor_mission_brief.md` for the complete transformation plan.
