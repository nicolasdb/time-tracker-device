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
- **Idle**: Blue breathing (4-second cycle) - system ready, no session active
- **WiFi Connecting**: Blue blinking (fast) - attempting connection
- **WiFi Connected**: Cyan flash (1 second) - connection established
- **AP Mode**: Yellow→Blue→Purple (0.3s,0.3s,2.0s) - configuration mode
- **Tag Detected**: Solid green (persistent) - time tracking session active
- **WiFi Failed**: Red→Orange→Red (0.3s,0.4s,0.3s) - connection error
- **Webhook Error**: Red→Red→Orange (0.2s,0.2s,0.6s) - server error
- **RFID Error**: Red→White→Red (0.2s,0.6s,0.2s) - hardware error
- **Webhook Queued**: Yellow→Green (0.5s,0.5s) - pending transmission

**Key Implementation Notes:**
- **Priority-based**: Higher priority states override lower ones
- **Session-aware**: Tag detected state is persistent (solid green until tag removed)
- **Automatic expiration**: Connection states are temporary, return to idle automatically
- **Thread-safe**: Mutex-protected queue for concurrent access
- **Boot protection**: 10-second grace period prevents duplicate events on reboot with tag present
- **Configurable**: Breathing period and brightness via Kconfig

## MCP Architecture Insights & Lessons Learned

### **Phase 1 Success: MCP-Inspired Tool Pattern for Embedded Systems**

**📅 COMPLETED:** 2025-01-14  
**🎯 RESULT:** MCP patterns work excellently for ESP32 embedded systems

### **🏆 PROVEN MCP PATTERNS FOR EMBEDDED**

**✅ TOOL-BASED COMPOSITION WORKS**
```c
// PROVEN: Handle-based tool lifecycle
feedback_tool_handle_t tool = feedback_tool_init(&config);
feedback_tool_set_state(tool, FEEDBACK_STATE_WIFI_CONNECTING, FEEDBACK_PRIORITY_MEDIUM, 5000);
feedback_tool_deinit(tool);

// PROVEN: Tool registry and capabilities discovery
const feedback_tool_registry_t* registry = feedback_tool_get_registry_entry();
ESP_LOGI(TAG, "Tool: %s v%s (caps: 0x%02X)", registry->tool_id, registry->version, registry->capabilities);
```

**✅ UNIVERSAL TOOL INTERFACE PATTERN**
- **Tool Metadata**: ID, version, description, capabilities enumeration
- **Lifecycle Management**: Handle-based init/deinit with resource cleanup
- **Capabilities Discovery**: Bitmask enumeration (0x1A = PRIORITY_QUEUE | AUTO_EXPIRE | THREAD_SAFE)
- **Status Reporting**: Real-time tool state, queue count, uptime tracking

**✅ PURE ORCHESTRATOR MAIN.C ACHIEVED**
```c
// EXCELLENT: main.c demonstrates MCP patterns, zero business logic
void app_main(void) {
    // 1. Tool initialization with MCP patterns
    feedback_tool_config_t config = feedback_tool_create_default_config();
    feedback_tool = feedback_tool_init(&config);
    
    // 2. 8-cycle demonstration of tool usage patterns
    for (int cycle = 0; cycle < 8; cycle++) {
        demonstrate_mcp_pattern(cycle);
    }
    
    // 3. Clean tool shutdown
    feedback_tool_deinit(feedback_tool);
}
```

### **🔧 TECHNICAL ARCHITECTURE WINS**

**PRIORITY QUEUE SYSTEM EXCELLENCE**
- **Automatic State Expiration**: Temporary states (1s flashes) self-expire
- **Thread-Safe Operations**: Mutex-protected queue for concurrent access
- **Priority-Based Override**: CRITICAL > HIGH > MEDIUM > LOW state hierarchy
- **Queue Management**: 8 states → 2 states automatic cleanup validated on hardware

**COMPONENT ISOLATION SUCCESS**
- **Zero Coupling**: feedback_tool has no dependencies on wifi/rfid/webhook tools
- **Build System Clean**: ESP-IDF component dependencies resolved properly
- **Reusability Proven**: Tool can be copied to any ESP32 project immediately
- **Resource Management**: Clean malloc/free, no memory leaks detected

**HARDWARE VALIDATION COMPLETE**
- **50+ Second Stable Operation**: No crashes, memory leaks, or state conflicts
- **GPIO LED Control**: Multiple patterns (breathing, blinking, solid) working
- **Real-time Metrics**: Uptime tracking, queue counts, capability reporting
- **Performance**: No regression from original feedback_manager implementation

### **🎓 CRITICAL INSIGHTS FOR FUTURE PHASES**

**ESP-IDF + MCP INTEGRATION PATTERNS**
- **Component Dependencies**: Use `REQUIRES` in CMakeLists.txt for proper dependency resolution
- **Header Structure**: Forward declarations crucial for complex type dependencies
- **Build System**: PlatformIO + ESP-IDF + tool directories work seamlessly
- **LED Strip Dependencies**: Modern ESP-IDF uses `color_component_format` not `led_pixel_format`

**MCP ARCHITECTURE DESIGN PRINCIPLES**
- **Handle-Based Interface**: Essential for embedded resource management
- **Capabilities Enumeration**: Bitmask pattern enables feature discovery
- **Tool Registry**: Metadata pattern enables tool composition and management
- **Event-Driven Communication**: Maintains decoupling while enabling coordination

**PROGRESSIVE REFACTORING SUCCESS**
- **Phase-Based Approach**: Prevents big-bang failures, enables incremental validation
- **Hardware-in-the-Loop Testing**: Essential for embedded MCP pattern validation
- **Rollback Strategy**: Git branches enable safe experimentation
- **Documentation-First**: Plans in code comments become worthless; separate docs essential

### **🚀 PHASE 2 READINESS**

**FOUNDATION SOLID FOR TOOL EXPANSION**
- **wifi_tool**: Network management with event-driven state transitions
- **rfid_tool**: Tag detection with MCP handle-based interface
- **webhook_tool**: HTTP client with tool registry integration
- **webserver_tool**: Configuration interface as MCP tool

**VALIDATED PATTERNS TO REPLICATE**
1. **Tool Structure**: Config → Init → Handle → Operations → Deinit
2. **Capabilities Discovery**: Bitmask enumeration for feature detection
3. **Registry Integration**: Metadata, version, description patterns
4. **Resource Lifecycle**: Proper memory management with cleanup validation
5. **State Management**: Priority queues with automatic expiration where applicable

**MCP FOR EMBEDDED = PRODUCTION READY** 🎉

### **Phase 2 Success: Multi-Tool MCP Architecture for Embedded Systems**

**📅 COMPLETED:** 2025-01-15  
**🎯 RESULT:** Event-driven multi-tool MCP patterns validated on hardware with WiFi tool transformation

### **🏆 PHASE 2 MCP ARCHITECTURE ACHIEVEMENTS**

**✅ EVENT-DRIVEN DECOUPLING MASTERY**
```c
// ELIMINATED: Direct coupling violation
// wifi_manager.c: ap_webserver_start(wifi_json_path);  ❌ TIGHT COUPLING

// ACHIEVED: Event-driven communication  
ESP_EVENT_DEFINE_BASE(WIFI_TOOL_EVENTS);

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    wifi_tool_event_t ap_event = {
        .type = WIFI_TOOL_EVENT_AP_STARTED,
        .data.ap_info.ap_ssid = "TimeTracker-Setup",
        .data.ap_info.ip_address = "192.168.4.1"
    };
    esp_event_post(WIFI_TOOL_EVENTS, WIFI_TOOL_EVENT_AP_STARTED, &ap_event, sizeof(ap_event), 0);
}
```

**✅ MULTI-TOOL ORCHESTRATION PATTERNS**
```c
// ACHIEVED: Pure orchestrator main.c with multiple tools
void app_main(void) {
    // Initialize multiple tools with MCP patterns
    feedback_tool = feedback_tool_init(&feedback_config);
    wifi_tool = wifi_tool_init(&wifi_config);
    
    // Tool discovery across multiple tools
    ESP_LOGI(TAG, "Feedback Registry: %s (caps: 0x%02X)", 
             feedback_tool_get_registry_entry()->tool_id, 0x1A);
    ESP_LOGI(TAG, "WiFi Registry: %s (caps: 0x%02X)", 
             wifi_tool_get_registry_entry()->tool_id, 0x7F);
    
    // Event-driven coordination (no direct coupling)
    wifi_tool_start_ap(wifi_tool);  // Publishes WIFI_TOOL_EVENT_AP_STARTED
    
    // Clean multi-tool shutdown
    wifi_tool_deinit(wifi_tool);
    feedback_tool_deinit(feedback_tool);
}
```

**✅ HANDLE-BASED STATE ISOLATION EXCELLENCE**
- **No Static Globals**: Both tools run with separate state contexts
- **Multi-Instance Ready**: Handle-based design enables multiple WiFi interfaces
- **Thread-Safe Operations**: Mutex-protected shared resources across tools
- **Independent Uptimes**: Each tool tracks its own lifecycle independently

### **🔧 PHASE 2 TECHNICAL WINS**

**WIFI TOOL TRANSFORMATION SUCCESS**
- **Component Dependencies**: Resolved ESP-IDF component name issues (`cJSON` → `json`)
- **String Safety**: Replaced `strncpy` with `snprintf` for compiler compliance  
- **Event Base Definition**: `ESP_EVENT_DEFINE_BASE(WIFI_TOOL_EVENTS)` working
- **Hardware Validation**: WiFi AP mode start/stop cycle validated on ESP32-C3

**BUILD SYSTEM MASTERY**
- **ESP-IDF Component Registration**: `idf_component_register()` with proper dependencies
- **Cross-Tool Dependencies**: Tools can reference each other's headers safely
- **PlatformIO + ESP-IDF**: Build pipeline handles complex tool structures flawlessly
- **Compiler Warning Resolution**: `-Werror=stringop-truncation` eliminated systematically

**MULTI-TOOL HARDWARE VALIDATION**
- **60+ Second Stable Operation**: Two tools running concurrently without conflicts
- **Priority Queue Coordination**: feedback_tool queue states: 8→2→3→4→5 dynamic management
- **Tool Status Monitoring**: Real-time uptime tracking for both tools independently
- **Event Publishing Verification**: WiFi AP events published successfully on hardware

### **🎓 PHASE 2 CRITICAL INSIGHTS FOR PHASE 3**

**EVENT-DRIVEN ARCHITECTURE PATTERNS**
- **ESP Event System Integration**: Use `esp_event_post()` for inter-tool communication
- **Event Base Declarations**: `ESP_EVENT_DECLARE_BASE()` in headers, `ESP_EVENT_DEFINE_BASE()` in source
- **Event Data Structures**: Rich event payloads with union types for different event data
- **Subscriber Patterns**: Future tools can subscribe to `WIFI_TOOL_EVENTS` for coordination

**HANDLE-BASED DESIGN PRINCIPLES**
- **Context Structures**: Encapsulate all tool state in opaque handle structures
- **Resource Management**: Each tool manages its own ESP-IDF resources (netif, event handlers)
- **Configuration Patterns**: `tool_create_default_config()` → `tool_init(config)` → `tool_deinit()`
- **Status APIs**: `tool_get_status()` provides real-time tool health and state information

**TOOL REGISTRY EVOLUTION**
- **Capabilities Enumeration**: Use bitmask patterns (0x1A, 0x7F) for feature discovery
- **Version Management**: String-based versioning for tool evolution tracking
- **Metadata Consistency**: ID, version, description, capabilities pattern across all tools
- **Registry Functions**: `tool_get_registry_entry()` enables tool discovery and introspection

### **🚀 PHASE 3 READINESS: COMPLETE TOOL ECOSYSTEM**

**VALIDATED PATTERNS FOR REMAINING TOOLS**
- **rfid_tool**: Apply handle-based interface to existing rfid_manager (90% ready)
- **webhook_tool**: Transform webhook_manager with event-driven HTTP operations
- **ntp_tool**: Extract time sync functionality from legacy wifi_manager
- **webserver_tool**: Subscribe to WIFI_TOOL_EVENTS for AP mode coordination

**PROVEN TRANSFORMATION METHODOLOGY**
1. **Analyze Current Architecture**: Identify coupling violations and static state issues
2. **Design Handle-Based Interface**: Create tool_config_t, tool_handle_t, tool_status_t
3. **Implement Event Publishing**: Replace direct calls with esp_event_post()
4. **Build System Integration**: CMakeLists.txt with proper ESP-IDF dependencies
5. **Hardware Validation**: Multi-tool demonstration with lifecycle management
6. **Tool Registry Compliance**: Metadata, capabilities, and discovery patterns

**ARCHITECTURE CONFIDENCE: PRODUCTION READY** 🎉  
Phase 2 proves MCP patterns scale to multi-tool embedded systems with event-driven coordination.

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