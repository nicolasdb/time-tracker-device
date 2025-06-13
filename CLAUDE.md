# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-C3 based RFID time tracking system using ESP-IDF framework. The device reads RFID/NFC tags and sends placement/removal events to a webhook server. It features WiFi connectivity, web-based configuration, LED feedback, and persistent storage via LittleFS.

## Build System & Commands

**IMPORTANT**: Claude should NOT execute build commands directly. The user handles building, flashing, and monitoring through VSCode + PlatformIO GUI.

### User's Role (VSCode + PlatformIO)
- **Building**: User builds via PlatformIO GUI in VSCode
- **Flashing**: User flashes via PlatformIO GUI in VSCode  
- **Monitoring**: User monitors hardware via PlatformIO serial monitor
- **Configuration**: User runs `idf.py menuconfig` when needed

### Claude's Role
- **Code analysis and modifications only**
- **Request user to build/test after changes**
- **Provide guidance on what to test**
- **Never execute build commands directly**

### ESP-IDF Commands (For Reference)
```bash
# Build the project
idf.py build

# Flash to device
idf.py flash

# Monitor serial output
idf.py monitor

# Flash and monitor in one command
idf.py flash monitor

# Clean build
idf.py fullclean

# Configure project settings
idf.py menuconfig

# Build for specific target (if needed)
idf.py set-target esp32c3
```

### PlatformIO Commands (For Reference)
```bash
# Build
pio run

# Flash
pio run --target upload

# Monitor
pio device monitor

# Clean
pio run --target clean
```

## Architecture Overview

The project follows ESP-IDF component-based architecture with modular design:

### Core Components Location
- **main/**: Application entry point and orchestration logic
- **components/**: Custom ESP-IDF components
  - **wifi_manager/**: WiFi connectivity, AP mode, NTP synchronization
  - **rfid_manager/**: RFID/NFC reading interface (RC522 support)
  - **webhook_manager/**: HTTP client for event transmission with retry logic
  - **feedback_manager/**: LED status indicators and visual feedback
  - **ap_webserver/**: Web configuration interface for WiFi setup

### Key Architectural Patterns

1. **Component-Based Design**: Each major functionality is isolated in its own ESP-IDF component with clear interfaces
2. **Event-Driven Architecture**: Uses FreeRTOS events and callbacks for inter-component communication
3. **Configuration Management**: Dual-layer approach with compile-time Kconfig and runtime LittleFS JSON configs
4. **State Management**: Priority-based queue system with event-driven state transitions
5. **Feedback System**: Visual LED patterns for different system states and error conditions

### Critical Architecture Lessons Learned

**⚠️ AVOID POLLING-BASED STATE MANAGEMENT**
- **Problem**: Main loop polling `wifi_manager_is_connected()` caused race conditions
- **Solution**: Use ESP event handlers (`WIFI_EVENT`, `IP_EVENT`) for state transitions
- **Principle**: Let events drive state changes, not polling loops

**✅ EVENT-DRIVEN FEEDBACK PATTERN**
```c
// GOOD: Event-driven state management
esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);

// BAD: Polling-based state checking
while(1) {
    if (wifi_manager_is_connected() != last_connected) {
        // This causes race conditions and state thrashing
    }
}
```

**🎯 PRIORITY-BASED FEEDBACK QUEUE**
- **Pattern**: High-priority states (errors, tag events) override low-priority (idle)
- **Implementation**: Sorted queue with automatic expiration for temporary states
- **Benefits**: No state conflicts, clear visual hierarchy, automatic cleanup

### Data Flow
1. **Hardware Events**: RFID/WiFi hardware → ESP event system → Component handlers
2. **State Transitions**: Event handlers → Feedback manager priority queue → LED patterns
3. **Data Processing**: RFID events → Main.c processing → Webhook payload creation
4. **Network Operations**: Webhook manager queue → HTTP transmission → Retry logic
5. **Persistence**: All events logged to LittleFS → Retry capability + debugging

## Configuration Management

### Compile-Time Configuration (Kconfig)
Access via `idf.py menuconfig`:
- Navigate to "Time Tracker Configuration" for project-specific settings
- Component-specific configs under "Component config"
- Critical settings: GPIO pins, webhook URLs, retry counts, LED brightness

### Runtime Configuration (LittleFS JSON)
Configuration files stored in `/littlefs/`:
- `wifi.json`: Network credentials with multi-SSID support
- `webhook_config.json`: Runtime webhook settings (planned feature)
- `log.json`: Event log for retry mechanism

### Hardware Pin Configuration
Default GPIO assignments (configurable via Kconfig):
- RC522 RFID: SPI interface
- WS2812B LED: Single data pin
- Status indicators: Configurable GPIO

## Important Implementation Details

### Time Synchronization
- Device waits for NTP sync before starting RFID operations
- All timestamps are ISO 8601 formatted with timezone
- Time sync failure prevents event logging to ensure accurate timestamps

### Webhook Event Format
Events sent as JSON with fields: event, tag_uid, device_id, timestamp, tag_type, firmware_version, hardware

### Error Handling & Retry Logic
- Webhook failures stored in persistent log with configurable retry attempts
- Background task processes failed events periodically
- Visual feedback for different error states via LED colors

### WiFi Management
- **Event-driven**: Uses WIFI_EVENT and IP_EVENT handlers (not polling)
- Automatic fallback to AP mode for initial configuration
- Multi-SSID support with priority-based connection attempts
- Web interface at 192.168.4.1 when in AP mode

### LED Feedback System

**Visual State Patterns:**
- **Idle**: Blue breathing (4-second cycle) - system ready
- **WiFi Connecting**: Blue blinking (fast) - attempting connection
- **WiFi Connected**: Cyan flash (1 second) - connection established
- **AP Mode**: Yellow→Blue→Purple (0.3s,0.3s,2.0s) - configuration mode
- **Tag Detected**: Green flash (2 seconds) - RFID event
- **WiFi Failed**: Red→Orange→Red (0.3s,0.4s,0.3s) - connection error
- **Webhook Error**: Red→Red→Orange (0.2s,0.2s,0.6s) - server error
- **RFID Error**: Red→White→Red (0.2s,0.6s,0.2s) - hardware error
- **Webhook Queued**: Yellow→Green (0.5s,0.5s) - pending transmission

**Key Implementation Notes:**
- **Priority-based**: Higher priority states override lower ones
- **Automatic expiration**: Temporary states return to idle automatically
- **Thread-safe**: Mutex-protected queue for concurrent access
- **Configurable**: Breathing period and brightness via Kconfig

## Development Best Practices

### Debugging State Issues

**LED State Debugging:**
1. **Check current_state**: Look for `current_state=X` in feedback_manager task logs
2. **Verify queue count**: `queue_count=Y` shows how many states are queued
3. **Event flow**: Trace WiFi events (`WiFi STA started`, `WiFi got IP`, etc.)
4. **State transitions**: Look for `LED state changed to: X` messages

**Common Issues & Solutions:**
- **Stuck in connecting state**: Usually event handler registration missing
- **No breathing effect**: Check if state is actually IDLE (state=1) 
- **State thrashing**: Remove polling, ensure event-driven design
- **Race conditions**: Always register event handlers before starting managers

### Architecture Debugging

**Event Flow Analysis:**
```bash
# Filter for WiFi events
grep -E "(WiFi|IP_EVENT|LED state)" terminal_logs.txt

# Check state machine flow  
grep "current_state=" terminal_logs.txt

# Verify initialization sequence
grep "Stage [0-9]:" terminal_logs.txt
```

**Component Communication:**
- Each component should be self-contained with clear interfaces
- Use ESP event system for cross-component communication
- Avoid direct function calls between components when possible
- Main.c should orchestrate, not implement business logic

### Code Quality Guidelines

**State Management Rules:**
1. **Event-driven over polling**: Always prefer event handlers
2. **Priority-based queues**: Use priority levels for visual feedback
3. **Automatic cleanup**: Temporary states should self-expire
4. **Thread safety**: Always protect shared state with mutexes

**Error Handling Patterns:**
- **Visual feedback**: Every error state should have a distinct LED pattern
- **Persistence**: Log failures for retry and debugging
- **Graceful degradation**: System should continue operating despite component failures
- **Recovery mechanisms**: Automatic retry with backoff for transient failures

## Testing

The project uses ESP-IDF's testing framework. Component tests are located in `components/*/test/` directories. The RC522 component includes comprehensive PICC (card) testing.

**Functional Testing Checklist:**
- [ ] Blue breathing effect visible in idle state
- [ ] WiFi connection sequence: blink → flash → breathing  
- [ ] Tag detection: breathing → green → breathing
- [ ] AP mode pattern: Yellow→Blue→Purple timing
- [ ] Error patterns: Each error type shows distinct pattern
- [ ] State persistence: System maintains correct state during operation

Run tests using:
```bash
# Component-specific tests
idf.py build -C components/rc522/test
```